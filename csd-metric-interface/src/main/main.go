package main

import (
	"encoding/json"
	"fmt"
	"net"
	"context"
    "log"
	"strings"
	"strconv"
	"os"
	
	//grpc 관련
	"google.golang.org/grpc"
	pb "opencsd/src/config"
)	

// Metric 구조체 정의
type Metric struct {
	Id           	  int     `json:"id"`
	
	TotalCpuCapacity  int 	  `json:"totalCpuCapacity"`
	CpuUsage		  float64 `json:"cpuUsage"`
	CpuUsagePercent	  float64 `json:"cpuUsagePercent"`
	
	TotalMemCapacity  int     `json:"totalMemCapacity"`
	MemUsage		  int  	  `json:"memUsage"`
	MemUsagePercent	  float64 `json:"memUsagePercent"`
	
	TotalDiskCapacity int     `json:"totalDiskCapacity"`
	DiskUsage		  int     `json:"diskUsage"`
	DiskUsagePercent  float64 `json:"diskUsagePercent"`
	
	NetworkBandwidth  int     `json:"networkBandwidth"`
	NetworkRxData	  int     `json:"networkRxData"`
	NetworkTxData	  int     `json:"networkTxData"`
}

func main() {
	OpenCSD_METRIC_COLLECTOR_IP := os.Getenv("OpenCSD_METRIC_COLLECTOR_IP")
    OpenCSD_METRIC_COLLECTOR_PORT := os.Getenv("OpenCSD_METRIC_COLLECTOR_PORT")
	
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
	metric.Id = extractCSDId(clientAddr)

	// Unmarshal된 구조체 값 출력
	fmt.Printf("ID: %d\nTotalCpuCapacity: %d\nCpuUsage: %f\nCpuUsagePercent: %f\n", metric.Id, metric.TotalCpuCapacity, metric.CpuUsage, metric.CpuUsagePercent)
	fmt.Printf("TotalMemCapacity: %d\nMemUsage: %d\nMemUsagePercent: %f\n", metric.TotalMemCapacity, metric.MemUsage, metric.MemUsagePercent)
	fmt.Printf("TotalDiskCapacity: %d\nDiskUsage: %d\nDiskUsagePercent: %f\n", metric.TotalDiskCapacity, metric.DiskUsage, metric.DiskUsagePercent)
	fmt.Printf("NetworkBandwidth: %d\nNetworkRxUsage: %d\nNetworkTxUsage: %d\n", metric.NetworkBandwidth, metric.NetworkRxData, metric.NetworkTxData)
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
	request := &pb.CSDMetricRequest{Id : int32(metric.Id), 
		TotalCpuCapacity : int32(metric.TotalCpuCapacity), 
		CpuUsage : float64(metric.CpuUsage), 
		CpuUsagePercent: float64(metric.CpuUsagePercent), 
		TotalMemCapacity : int32(metric.TotalMemCapacity), 
		MemUsage : int32(metric.MemUsage),
		MemUsagePercent : float64(metric.MemUsagePercent), 
		TotalDiskCapacity : int32(metric.TotalDiskCapacity), 
		DiskUsage : int32(metric.DiskUsage), 
		DiskUsagePercent : float64(metric.DiskUsagePercent), 
		NetworkBandwidth : int32(metric.NetworkBandwidth), 
		NetworkRxData : int32(metric.NetworkRxData), 
		NetworkTxData : int32(metric.NetworkTxData)}
    
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