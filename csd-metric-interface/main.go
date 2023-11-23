package main

import (
	"encoding/json"
	"fmt"
	"net"
	"context"
    "log"
	"strings"
	"strconv"
	
	//grpc 관련
	"google.golang.org/grpc"
	pb "test_client/config"
)	

// Metric 구조체 정의
type Metric struct {
	id           int     `json:"id"`
	// CPUUsage     float64 `json:"cpuUsage"`
	// MemUsage     float64 `json:"memUsage"`
	// NetworkSpeed float64 `json:"networkSpeed"`
	
	totalCpuCapacity  float64 `json:"totalCpuCapacity"`
	cpuUsage		  float64 `json:"cpuUsage"`
	cpuUsagePercent	  float64 `json:"cpuUsagePercent"`
	
	totalMemCapacity  float64 `json:"totalMemCapacity"`
	memUsage		  float64 `json:"memUsage"`
	memUsagePercent	  float64 `json:"memUsagePercent"`
	
	totalDiskCapacity float64 `json:"totalDiskCapacity"`
	diskUsage		  float64 `json:"diskUsage"`
	diskUsagePercent  float64 `json:"diskUsagePercent"`
	
	networkBandwidth  float64 `json:"networkBandwidth"`
	networkRxData	  float64 `json:"networkRxUsage"`
	networkTxData	  float64 `json:"networkTxUsage"`
}

var(
	OpenCSD_METRIC_COLLECTOR_IP = "10.0.4.87" //OpenCSD Metric Collector grpc 서버
	OpenCSD_METRIC_COLLECTOR_PORT = "30003"
)

func main() {
	// TCP 서버 시작
	listener, err := net.Listen("tcp", ":40800")
	if err != nil {
		fmt.Println("Error starting the server:", err)
		return
	}
	defer listener.Close()

	fmt.Println("TCP server is listening on port 40800")

	for {
		// 클라이언트 연결 대기
		conn, err := listener.Accept()
		if err != nil {
			fmt.Println("Error accepting connection:", err)
			continue
		}
		var CSDMetric Metric
		// csd 메트릭 수신 in 10.0.4.83
		CSDMetricReceiver(conn, &CSDMetric)
		// grpc 서버에 메트릭 전송 to 10.0.4.87
		CSDMetricSender(OpenCSD_METRIC_COLLECTOR_IP, OpenCSD_METRIC_COLLECTOR_PORT, &CSDMetric)
	}
}

// csd 메트릭 수신
func CSDMetricReceiver(conn net.Conn, metric *Metric) {
	defer conn.Close()

	// 클라이언트로부터 JSON 데이터 수신
	buffer := make([]byte, 1024)
	n, err := conn.Read(buffer)
	if err != nil {
		fmt.Println("Error reading data:", err)
		return
	}

	// JSON 데이터 파싱하여 Metric 구조체에 저장
	// var metric Metric
	err = json.Unmarshal(buffer[:n], metric)
	if err != nil {
		fmt.Println("Error decoding JSON:", err)
		return
	}
	
	// csd id 생성
	clientAddr := conn.RemoteAddr().String()
	metric.id = extractCSDId(clientAddr)
	fmt.Printf("Client Connected from %s\n", clientAddr)

	// // 파싱된 데이터 출력
	// fmt.Printf("Received JSON Data:\nID: %d\nCPU Usage: %.2f\nMem Usage: %.2f\nNetwork Speed: %.2f\n",
	// 	metric.ID, metric.CPUUsage, metric.MemUsage, metric.NetworkSpeed)
		
    // 파싱된 데이터 출력
    fmt.Printf("Received JSON Data:\n"+
		"id: %d\n"+
        "Total CPU Capacity: %.2f\nCPU Usage: %.2f\nCPU Usage Percent: %.2f%%\n"+
        "Total Mem Capacity: %.2f\nMem Usage: %.2f\nMem Usage Percent: %.2f%%\n"+
        "Total Disk Capacity: %.2f\nDisk Usage: %.2f\nDisk Usage Percent: %.2f%%\n"+
        "Network Bandwidth: %.2f\nNetwork Rx Data: %.2f\nNetwork Tx Data: %.2f\n",
        metric.id,
		metric.totalCpuCapacity, metric.cpuUsage, metric.cpuUsagePercent,
        metric.totalMemCapacity, metric.memUsage, metric.memUsagePercent,
        metric.totalDiskCapacity, metric.diskUsage, metric.diskUsagePercent,
        metric.networkBandwidth, metric.networkRxData, metric.networkTxData)
}

// grpc 서버 접속 및 메트릭 전송
func CSDMetricSender(ip string, port string, metric *Metric) {
	// fmt.Printf("Received JSON Data:\nID: %d\nCPU Usage: %.2f\nMem Usage: %.2f\nNetwork Speed: %.2f\n",
	// metric.ID, metric.CPUUsage, metric.MemUsage, metric.NetworkSpeed)
	
	// gRPC 서버에 연결
    conn, err := grpc.Dial(ip + ":" + port, grpc.WithInsecure())
    if err != nil {
        log.Fatalf("Could not connect: %v", err)
    }
    defer conn.Close()

    // gRPC 클라이언트 생성
    client := pb.NewCSDMetricClient(conn)

	// gRPC 메서드 호출 => csd metric 기반 request 생성 및 전송 후 response 수신
	request := &pb.CSDMetricRequest{Id : int32(metric.id), 
		TotalCpuCapacity : float64(metric.totalCpuCapacity), 
		CpuUsage : float64(metric.cpuUsage), 
		CpuUsagePercent: float64(metric.cpuUsagePercent), 
		TotalMemCapacity : float64(metric.totalMemCapacity), 
		MemUsage : float64(metric.memUsage),
		MemUsagePercent : float64(metric.memUsagePercent), 
		TotalDiskCapacity : float64(metric.totalDiskCapacity), 
		DiskUsage : float64(metric.diskUsage), 
		DiskUsagePercent : float64(metric.diskUsagePercent), 
		NetworkBandwidth : float64(metric.networkBandwidth), 
		NetworkRxData : float64(metric.networkRxData), 
		NetworkTxData : float64(metric.networkTxData)}
    
	// grpc 서버 응답
	response, err := client.ReceiveCSDMetric(context.Background(), request)
    if err != nil {
        log.Fatalf("Get Response Error From Grpc Server: %v", err)
    }
    fmt.Printf("Response From gRPC Server: %s\n\n", response.JsonConfig) //응답 형식 확인해야 함
}

// IP 주소에서 CSD ID 추출
func extractCSDId(addr string) int {
	parts := strings.Split(addr, ".")
	if len(parts) > 0 {
		id := parts[2] // 세번째 필드값으로 id 설정
		Id, err := strconv.Atoi(id)
		if err != nil{
			fmt.Println("CSD Id Parsing Fail")
		}
		return Id
	}
	return 0
}