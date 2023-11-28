#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include "ip_config.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <sys/statvfs.h>
#include <chrono>
#include <thread>
#include <atomic>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

using namespace std;
using namespace rapidjson;

#define ONE_LINE 80
#define PAST 0
#define PRESENT 1
#define JIFFIES_NUM 4

#define CSD_METRIC_INTERFACE_IP "10.0.4.83"
#define CSD_METRIC_INTERFACE_PORT 30004

struct Result { //metric 수집 결과 저장
    float totalCpuCapacity, cpuUsage, cpuUsagePercent; // cpu
    
    int totalMemCapacity, memUsage; // memory
    float memUsagePercent;

    int totalDiskCapacity, diskUsage; // disk
    float diskUsagePercent; 

    float totalPowerCapacity, powerUsage, powerUsagePercent; // power

    long long networkUsage, networkTxData, networkRxData; // network

    double csdMetricScore; // metric score 

    std::string ip; // ip
};

// cpu
struct stJiffies
{
    int user;
    int nice;
    int system;
    int idle;
};

// memory
struct MemoryInfo {
    long MemTotal;
    long MemFree;
    long Buffers;
    long Cached;
};

class MetricCollector
{

    private:
        stJiffies curJiffies, prevJiffies, diffJiffies; // cpu
        long long initialRxBytes, initialTxBytes, currentRxBytes, currentTxBytes; // network
        long long initialEnergy1, initialEnergy2, currentEnergy1, currentEnergy2;// power

    public:
        thread runThread;

        MetricCollector(){
            cpuUsageInit();
            networkUsageInit();
            this_thread::sleep_for(chrono::seconds(5));
        }

        string ip = "";

        void cpuUsageInit(){
            FILE *pStat = NULL;
            char cpuID[6] = {0};
            
            pStat = fopen("/metric/cpuMemUsage/stat", "r");
            fscanf(pStat, "%s %d %d %d %d", cpuID, &prevJiffies.user,
            &prevJiffies.nice, &prevJiffies.system, &prevJiffies.idle);

            fclose(pStat);
        }

        void networkUsageInit(){
            string statisticsFilePath = "/metric/networkUsage/";
            string rxBytesFieldName = statisticsFilePath + "rx_bytes";
            string txBytesFieldName = statisticsFilePath + "tx_bytes";

            string initialRxBytesStr = readStatisticsField(rxBytesFieldName);
            string initialTxBytesStr = readStatisticsField(txBytesFieldName);

            initialRxBytes = std::stoll(initialRxBytesStr);
            initialTxBytes = std::stoll(initialTxBytesStr);
        }

        void powerUsageInit(){
            string powerFilePath1 = "intel-rapl:0";
            string powerFilePath2 = "intel-rapl:1";

            string energyFieldName1 = "/sys/class/powercap/" + powerFilePath1 + "/energy_uj"; 
            string energyFieldName2 = "/sys/class/powercap/" + powerFilePath2 + "/energy_uj"; 

            string initialEnergyStr1 = readStatisticsField(energyFieldName1);
            string initialEnergyStr2 = readStatisticsField(energyFieldName2);

            initialEnergy1 = std::stoll(initialEnergyStr1);
            initialEnergy2 = std::stoll(initialEnergyStr2);

        }

        string readStatisticsField(const string &fieldName) const {
            ifstream file(fieldName);
            string line;
            string value;
            file >> value; // 파일에서 값을 읽어옴
            return value;
        }

        void getIPAddress(Result *result) {
            const char* csd_ip = std::getenv("CSD_IP");

            if (csd_ip) {
                std::cout << "CSD_IP environment variable: " << csd_ip << std::endl;
                result->ip = csd_ip;
            } else {
                std::cout << "CSD_IP environment variable not set." << std::endl;
            }
        }

        void getCpuUsage(Result *result)
        {
            FILE *pStat = NULL;
            char cpuID[6] = {0};

            pStat = fopen("/metric/cpuMemUsage/stat", "r");
            fscanf(pStat, "%s %d %d %d %d", cpuID, &curJiffies.user,
                    &curJiffies.nice, &curJiffies.system, &curJiffies.idle);

            diffJiffies.user =  curJiffies.user - prevJiffies.user;
            diffJiffies.nice =  curJiffies.nice - prevJiffies.nice;
            diffJiffies.system =  curJiffies.system - prevJiffies.system;
            diffJiffies.idle =  curJiffies.idle - prevJiffies.idle;

            int totalJiffies = diffJiffies.user + diffJiffies.nice + diffJiffies.system + diffJiffies.idle;
            prevJiffies = curJiffies;
            fclose(pStat);
            result->cpuUsagePercent = 100.0f * (1.0-(diffJiffies.idle / (double) totalJiffies));

            // cpu 코어 수
            FILE* fp = popen("grep -c processor /proc/cpuinfo", "r");
            int coreCount;
            
            if (!fp) {
                std::cerr << "Error: popen failed." << std::endl;
                // return -1;
            }
            
            if (fscanf(fp, "%d", &coreCount) != 1) {
                std::cerr << "Error: Failed to read CPU core count." << std::endl;
                pclose(fp);
                // return -1;
            }
            result->totalCpuCapacity = coreCount;
            pclose(fp);

            // 현재 cpu 코어 사용량
            result->cpuUsage = (float)result->totalCpuCapacity * (1.0-(diffJiffies.idle / (double) totalJiffies));
            // return result->cpuUsage;
        }

        void getMemUsage(Result *result) {
            std::ifstream meminfo("/metric/cpuMemUsage/meminfo");
            MemoryInfo info;

            std::string line;
            while (std::getline(meminfo, line)) {
                std::istringstream iss(line);
                std::string key;
                long value;

                iss >> key >> value;

                if (key == "MemTotal:") {
                    info.MemTotal = value;
                } else if (key == "MemFree:") {
                    info.MemFree = value;
                } else if (key == "Buffers:") {
                    info.Buffers = value;
                } else if (key == "Cached:") {
                    info.Cached = value;
                }
            }
            
            result->totalMemCapacity = info.MemTotal; 
            result->memUsage = info.MemTotal - info.MemFree - info.Buffers - info.Cached;
            result->memUsagePercent = (1.0 - static_cast<double>(info.MemFree + info.Buffers + info.Cached) / info.MemTotal) * 100.0;
            
            // return result->memUsagePercent;
        }

        void getDiskUsage(Result *result) {
            // DiskInfo diskInfo;
            // Run the df command and read its output
            std::string dfCommand = "df -k --total";
            std::string dfOutput;
            FILE* pipe = popen(dfCommand.c_str(), "r");
            char buffer[128];
            
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                dfOutput += buffer; //디스크 분석 결과 기록
            }
            pclose(pipe);

            std::istringstream stream(dfOutput); // 커맨드 결과 파싱
            std::string line;

            std::getline(stream, line); // 처음 줄 skip

            
            while (std::getline(stream, line)) { // 마지막 줄에서 사용 및 전체 용량 추출
                std::istringstream lineStream(line);
                std::string skip;
                lineStream >> skip >> result->totalDiskCapacity >> result->diskUsage >> skip >> skip;
            }

            result->diskUsagePercent = static_cast<float>(result->diskUsage) / result->totalDiskCapacity * 100;
            // return result->diskUsagePercent;    
        }

        void getNetworkSpeed(Result *result)
        {
            string statisticsFilePath = "/metric/networkUsage/";
            // string statisticsFilePath = "/sys/class/net/eno1/statistics/";
            
            string rxBytesFieldName = statisticsFilePath + "rx_bytes";
            string txBytesFieldName = statisticsFilePath + "tx_bytes";

            string currentRxBytesStr = readStatisticsField(rxBytesFieldName);
            string currentTxBytesStr = readStatisticsField(txBytesFieldName);
            
            currentRxBytes = std::stoll(currentRxBytesStr);
            currentTxBytes = std::stoll(currentTxBytesStr);

            // 네트워크 송신량
            result->networkTxData = currentTxBytes-initialTxBytes;
            
            // 네트워크 수신량 
            result->networkRxData = currentRxBytes-initialRxBytes;
            
            // 대역폭()
            // result->networkUsage = (currentRxBytes-initialRxBytes) + (currentTxBytes-initialTxBytes);
            result->networkUsage = ((currentRxBytes-initialRxBytes) + (currentTxBytes-initialTxBytes)) / 5 * 8;
            
            initialRxBytes = currentRxBytes;
            initialTxBytes = currentTxBytes;
   
            // return result->networkUsage;
        }
        
        float getPowerUsage(Result *result){
            string powerFilePath1 = "intel-rapl:0";
            string powerFilePath2 = "intel-rapl:1";

            string energyFieldName1 = "/sys/class/powercap/" + powerFilePath1 + "/energy_uj"; 
            string energyFieldName2 = "/sys/class/powercap/" + powerFilePath2 + "/energy_uj"; 

            string initialEnergyStr1 = readStatisticsField(energyFieldName1);
            string initialEnergyStr2 = readStatisticsField(energyFieldName2);

            initialEnergy1 = std::stoll(initialEnergyStr1);
            initialEnergy2 = std::stoll(initialEnergyStr2);

            //현재 파워 사용량
            result->powerUsage = ((currentEnergy1 - initialEnergy1) + (currentEnergy2 - initialEnergy2));

        }

        void getCSDMetricScore(Result *result){
            // 가중치 설정
            const double cpuWeight = 0.33;
            const double memoryWeight = 0.33;
            const double diskWeight = 0.33;
            // const double networkWeight = 0.25;

            // 총 점수 계산
            double totalScore = cpuWeight * (100 - result->cpuUsagePercent) +
                                memoryWeight * (100 - result->memUsagePercent) +
                                diskWeight * (100 - result->diskUsagePercent);
                                // diskWeight * result->diskUsagePercent +
                                // networkWeight * (1 - result->networkUsage);

            result->csdMetricScore = totalScore;
        }

        int workingBlockCount = 0;

        void serialize(StringBuffer &buff, Result &result)
        { 
            Writer<StringBuffer> writer(buff);
            
            writer.StartObject(); // metric 데이터 json화
            writer.Key("ip");
            writer.String(result.ip.c_str());

            writer.Key("totalCpuCapacity");
            writer.Int(result.totalCpuCapacity);
            writer.Key("cpuUsage");
            writer.Double(result.cpuUsage); 
            writer.Key("cpuUsagePercent");
            writer.Double(result.cpuUsagePercent);

            writer.Key("totalMemCapacity");
            writer.Int(result.totalMemCapacity);
            writer.Key("memUsage");
            writer.Int(result.memUsage);
            writer.Key("memUsagePercent");
            writer.Double(result.memUsagePercent);

            writer.Key("totalDiskCapacity");
            writer.Int(result.totalDiskCapacity);
            writer.Key("diskUsage");
            writer.Int(result.diskUsage);
            writer.Key("diskUsagePercent");
            writer.Double(result.diskUsagePercent);

            writer.Key("networkRxData");
            writer.Int(result.networkRxData);
            writer.Key("networkTxData");
            writer.Int(result.networkTxData);
            writer.Key("networkBandwidth");
            writer.Int(result.networkUsage);

            writer.Key("csdMetricScore");
            writer.Double(result.csdMetricScore);
            writer.EndObject();
        }

        void run()
        { 
            int workingBlockCount = 0;
            float cpuUsage, memUsage, diskUsage, networkSpeed = 0;
            char ipBuffer[INET_ADDRSTRLEN];

            // TCP 소켓 생성
            StringBuffer buff;
            int sock = socket(AF_INET, SOCK_STREAM, 0);      

            // 서버 주소 생성 및 tcp/ip 서버 연결
            struct sockaddr_in serv_addr;
            memset(&serv_addr, 0, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_addr.s_addr = inet_addr(CSD_METRIC_INTERFACE_IP); // ServerIP
            serv_addr.sin_port = htons(CSD_METRIC_INTERFACE_PORT);          // ServerPort
            
            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
            {
                cout << "tcp/ip server connect error" << endl;
                exit(1);
            }

            struct sockaddr_in localAddress
            {
            };
            socklen_t addressLength = sizeof(localAddress);

            if (getsockname(sock, (struct sockaddr *)&localAddress, &addressLength) == -1)

            char ipBuffer[INET_ADDRSTRLEN];
            const char *clientIP = inet_ntop(AF_INET, &(localAddress.sin_addr), ipBuffer, INET_ADDRSTRLEN);

            // metric collect
            Result result;

            getIPAddress(&result);
            getCpuUsage(&result);
            getMemUsage(&result);
            getDiskUsage(&result);
            getNetworkSpeed(&result);
            // getPowerUsage(&result);
            getCSDMetricScore(&result);
            serialize(buff, result);
            
            // 출력
            std::cout << "ip: " << result.ip << std::endl;
            std::cout << "\n" << std::endl;
            std::cout << "Total CPU Capacity: " << result.totalCpuCapacity << std::endl;
            std::cout << "CPU Usage: " << result.cpuUsage << std::endl;
            std::cout << "CPU Usage Percent: " << result.cpuUsagePercent << "%" << std::endl;
            std::cout << "\n" << std::endl;
            std::cout << "Total Memory Capacity(KB): " << result.totalMemCapacity << std::endl;
            std::cout << "Memory Usage(KB): " << result.memUsage << std::endl;
            std::cout << "Memory Usage Percent(%): " << result.memUsagePercent << "%" << std::endl;
            std::cout << "\n" << std::endl;
            std::cout << "Total Disk Capacity(KB): " << result.totalDiskCapacity << std::endl;
            std::cout << "Disk Usage(KB): " << result.diskUsage << std::endl;
            std::cout << "Disk Usage Percent(%): " << result.diskUsagePercent << "%" << std::endl;
            std::cout << "\n" << std::endl;
            // std::cout << "Total Power Capacity: " << result.totalPowerCapacity << std::endl;
            // std::cout << "Power Usage: " << result.powerUsage << std::endl;
            // std::cout << "Power Usage Percent: " << result.powerUsagePercent << "%" << std::endl;
            // std::cout << "\n" << std::endl;
            // std::cout << "Total Network Capacity: " << result.totalNetworkCapacity << std::endl;
            std::cout << "Network Received Usage(Byte): " << result.networkRxData << std::endl;
            std::cout << "Network Send Usage(Byte): " << result.networkTxData << std::endl;
            std::cout << "Network Usage(kbps): " << result.networkUsage << std::endl;
            std::cout << "\n" << std::endl;      

            std::cout << "CSD Metric Score: " << result.csdMetricScore << std::endl;  
            std::cout << "\n" << std::endl;

            // JSON 데이터를 문자열로 변환
            const char* jsonStr = buff.GetString();
            
            // socket 보낸 후, 닫기
            send(sock, jsonStr, strlen(jsonStr), 0);
            close(sock);
        }
};

int cnt = 1;

int main(int argc, char *argv[])
{
    MetricCollector metricCollector;
    
    while (true)
    {
        std::thread t1(&MetricCollector::run, &metricCollector);
        t1.join();
        this_thread::sleep_for(chrono::seconds(5));
    }
    // 소켓 닫기

    return 0;
}