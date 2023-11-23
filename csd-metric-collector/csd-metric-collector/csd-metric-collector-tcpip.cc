#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "ip_config.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <sys/statvfs.h>
#include <chrono>
#include <thread>
#include <atomic>

#include "stdlib.h"
#include "stdio.h"
#include "string.h"

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

#define CSD_METRIC_INTERFACE_IP "10.0.4.87"
#define CSD_METRIC_INTERFACE_PORT 40702

class MetricCollector
{

public:
    thread networkSpeedThread; // 네트워크 속도를 측정하는 쓰레드
    thread workingBlockCountThread;
    thread runThread;
    
    enum jiffy
    {
        USER,
        USER_NICE,
        SYSTEM,
        IDLE
    } jiffy_enum;

    struct Metric
    {
        string ip = "";
        // float getCpuUsage() const
        // {
        //     char loadDataBuf[ONE_LINE] = {0};
        //     char cpuId[4] = {0};
        //     float cpuUsage;
        //     int jiffies[2][JIFFIES_NUM] = {0}, totalJiffies;
        //     int diffJiffies[JIFFIES_NUM];
        //     int idx;
        //     FILE *statFile;
        //     statFile = fopen("/proc/stat", "r");
        //     fscanf(statFile, "%s %d %d %d %d", cpuId, &jiffies[PRESENT][USER], &jiffies[PRESENT][USER_NICE],
        //            &jiffies[PRESENT][SYSTEM], &jiffies[PRESENT][IDLE]);

        //     for (idx = 0, totalJiffies = 0; idx < JIFFIES_NUM; ++idx)
        //     {
        //         diffJiffies[idx] = jiffies[PRESENT][idx] - jiffies[PAST][idx];
        //         totalJiffies = totalJiffies + diffJiffies[idx];
        //     }

        //     cpuUsage = 100.0 * (1.0 - (diffJiffies[IDLE] / static_cast<double>(totalJiffies)));
        //     memcpy(jiffies[PAST], jiffies[PRESENT], sizeof(int) * JIFFIES_NUM);
        //     fclose(statFile);
        //     return cpuUsage;
        // }

        float getCpuUsage(){
            float percent;
            FILE* file;
            unsigned long long totalUser, totalUserLow, totalSys, totalIdle, total;

            file = fopen("/proc/stat", "r");
            fscanf(file, "cpu %llu %llu %llu %llu", &totalUser, &totalUserLow,
                &totalSys, &totalIdle);
            fclose(file);

            if (totalUser < lastTotalUser || totalUserLow < lastTotalUserLow ||
                totalSys < lastTotalSys || totalIdle < lastTotalIdle){
                //Overflow detection. Just skip this value.
                percent = -1.0;
            }
            else{
                total = (totalUser - lastTotalUser) + (totalUserLow - lastTotalUserLow) +
                    (totalSys - lastTotalSys);
                percent = total;
                total += (totalIdle - lastTotalIdle);
                percent /= total;
                percent *= 100;
            }

            lastTotalUser = totalUser;
            lastTotalUserLow = totalUserLow;
            lastTotalSys = totalSys;
            lastTotalIdle = totalIdle;

            return percent;
        }

        float getMemUsage() const
        {
            ifstream meminfoFile("/proc/meminfo");
            if (!meminfoFile)
            {
                cerr << "Failed to open /proc/meminfo" << endl;
                return 0.0;
            }

            string line;
            string memTotalStr, memAvailableStr;
            double memTotal = 0.0, memAvailable = 0.0;

            while (std::getline(meminfoFile, line))
            {
                if (line.find("MemTotal:") != string::npos)
                {
                    memTotalStr = line.substr(line.find(":") + 1);
                    memTotal = stod(memTotalStr);
                }
                else if (line.find("MemAvailable:") != string::npos)
                {
                    memAvailableStr = line.substr(line.find(":") + 1);
                    memAvailable = stod(memAvailableStr);
                }
            }

            if (memTotal == 0.0)
            {
                cerr << "Failed to parse MemTotal" << endl;
                return 0.0;
            }

            float memoryUsage = (memTotal - memAvailable) / memTotal * 100.0;
            return memoryUsage;
        }
        // float getDiskUsage() const
        // {
        //     struct statvfs stat;
        //     if (statvfs("/", &stat) != 0)
        //     {
        //         cerr << "Failed to get file system statistics." << endl;
        //         return 1;
        //     }
        //     uint64_t total_space = stat.f_blocks * stat.f_frsize;
        //     uint64_t free_space = stat.f_bfree * stat.f_frsize;
        //     uint64_t used_space = total_space - free_space;
        //     float diskUsage = static_cast<float>(used_space) / total_space * 100;

        //     return diskUsage;
        // }
        // float getNetworkSpeed() const
        // {
        //     float networkSpeed = 0;
        //     string statisticsFilePath = "/sys/class/net/ngdtap0/statistics/";
        //     //string statisticsFilePath = "/sys/class/net/eno1/statistics/";
        //     string rxBytesFieldName = "rx_bytes";
        //     string txBytesFieldName = "tx_bytes";

        //     // Function to read statistics file and retrieve value of a specific field

        //     auto readStatisticsField = [&](const string &fieldName)
        //     {
        //         ifstream file(statisticsFilePath + fieldName);
        //         string line;

        //         string value;
        //         file >> value; // 파일에서 값을 읽어옴
        //         return value;
        //     };

        //     // Read initial values
        //     string initialRxBytesStr = readStatisticsField(rxBytesFieldName);
        //     string initialTxBytesStr = readStatisticsField(txBytesFieldName);
        //     // Read current values
        //     this_thread::sleep_for(chrono::seconds(2));

        //     string currentRxBytesStr = readStatisticsField(rxBytesFieldName);
        //     string currentTxBytesStr = readStatisticsField(txBytesFieldName);
        //     // Convert values to integers

        //     long long initialRxBytes = stoll(initialRxBytesStr);
        //     long long initialTxBytes = stoll(initialTxBytesStr);
        //     long long currentRxBytes = stoll(currentRxBytesStr);
        //     long long currentTxBytes = stoll(currentTxBytesStr);
        //     /*cout << " 1RxBytes = " << initialRxBytes
        //          << " 1TxBytes = " << initialTxBytes
        //          << " 2RxBytes = " << currentRxBytes
        //          << " 2TxBytes = " << currentTxBytes << endl;
        //     */
        //     // Calculate bytes transferred during the duration
        //     long long rxBytesTransferred = currentRxBytes - initialRxBytes;
        //     long long txBytesTransferred = currentTxBytes - initialTxBytes;

        //     // Calculate network speed in Mbps
        //     float rxSpeedKBps = (rxBytesTransferred * 8.0) / (2 * 1000);
        //     float txSpeedKBps = (txBytesTransferred * 8.0) / (2 * 1000);

        //     // cout << "RX Speed: " << rxSpeedMbps << " Mbps" << endl;
        //     // cout << "TX Speed: " << txSpeedMbps << " Mbps" << endl;
        //     networkSpeed = rxSpeedKBps + txSpeedKBps;
        //     // cout << "networkSpeed: " << networkSpeed <<endl;
        //     return networkSpeed;
        // }
        // int workingBlockCount = 0;
        // void workingBlock()
        {
            // 소켓 생성
            int server_sock, client_sock;
            int opt = 1;
            struct sockaddr_in server_addr;
            struct sockaddr_storage client_addr;
            socklen_t client_addr_size = sizeof(client_addr);

            // 서버 소켓 생성
            server_sock = socket(AF_INET, SOCK_STREAM, 0);
            if (server_sock == -1)
            {
                cout << "socket() error" << endl;
                exit(1);
            }
            if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
            {
                perror("setsockopt");
                exit(EXIT_FAILURE);
            }
            // 서버 주소 설정
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            server_addr.sin_port = htons(CSD_WORKING_BLOCK_PORT);
            
            // 소켓에 주소 바인딩
            if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
            {
                cout << "CSD-WORKER-MODULE bind() error" << endl;
                exit(1);
            }
            // cout << "bind csd worker module " << endl;
            //  연결 대기 상태 진입
            if (listen(server_sock, 5) < 0)
            {
                cout << "listen() error" << endl;
                exit(1);
            }
            //cout << "2. listening csd worker module" << endl;
            while (1)
            {
                // 연결 수락
                if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size)) < 0)
                {
                    cout << "accept error" << endl;
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                //cout << "3. accept csd worker module " << endl;

                int blockCount;
                recv(client_sock, &blockCount, sizeof(blockCount), 0);
                //cout << "받은 메세지" << buffer << endl;
                workingBlockCount = blockCount;
                close(client_sock);
            }
            close(server_sock);
        }
    };

    Metric metric;
    int workingBlockCount = 0;
    float cpuUsage, memUsage, diskUsage, networkSpeed = 0;
    char ipBuffer[INET_ADDRSTRLEN];

    // 3. metric 정보 json으로 tcp/ip 서버에 send
    void metricCollect()
    {
        cpuUsage = metric.getCpuUsage();
        memUsage = metric.getMemUsage();
        // diskUsage = metric.getDiskUsage();
        // networkSpeed = metric.getNetworkSpeed();
        // workingBlockCount = metric.workingBlockCount;
    }

    void serialize(StringBuffer &buff)
    { 
        Writer<StringBuffer> writer(buff);
        
        writer.StartObject(); // metric 데이터 json화
        writer.Key("ip");
        writer.String(metric.ip.c_str());
        writer.Key("cpuUsage");
        writer.Double(cpuUsage); 
        writer.Key("memUsage");
        writer.Double(memUsage);
        // writer.Key("diskUsage");
        // writer.Double(diskUsage);
        // writer.Key("networkSpeed");
        // writer.Double(networkSpeed);
        // writer.Key("workingBlockCount");
        // writer.Int(workingBlockCount);
        writer.EndObject();
    }
    void run()
    { 
        // TCP 소켓 생성
        StringBuffer buff;

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        
        // if (sock == -1)
        // {
        //     cout << "socket() error" << endl;
        //     exit(1);
        // }

        // 서버 주소 생성 및 tcp/ip 서버 연결
        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(CSD_METRIC_INTERFACE_IP); // ServerIP
        serv_addr.sin_port = htons(CSD_METRIC_INTERFACE_PORT);          // ServerPort
        
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        {
            cout << "connect() error" << endl;
            exit(1);
        }

        struct sockaddr_in localAddress
        {
        };
        socklen_t addressLength = sizeof(localAddress);

        if (getsockname(sock, (struct sockaddr *)&localAddress, &addressLength) == -1)
        {
            cerr << "Failed to get local address." << endl;
            exit(1);
        }

        char ipBuffer[INET_ADDRSTRLEN];
        const char *clientIP = inet_ntop(AF_INET, &(localAddress.sin_addr), ipBuffer, INET_ADDRSTRLEN);
        // if (clientIP == nullptr)
        // {
        //     cerr << "Failed to convert IP address to string." << endl;
        //     exit(1);
        // }
        // serialize(buff);
        // metric.ip = ipBuffer;
        
        // metric collect
        // metric.ip = ipBuffer;
        metricCollect();

        serialize(buff);
        // metric.ip = ipBuffer;
        // cout << "IP: " << metric.ip << endl;
        cout << fixed << setprecision(5); //소수점 5번 째 자리까지 출력
        cout << "CpuUsage(%): " << cpuUsage << endl;
        cout << "MemoryUsage(%): " << memUsage << endl;
        // cout << "DiskUsage(%): " << diskUsage << endl;
        // cout << "NetworkSpeed(KBps): " << networkSpeed << endl;
        // cout << "WorkingBlockCount: " << metric.workingBlockCount << endl;

        // JSON 데이터를 문자열로 변환
        const char* jsonStr = buff.GetString();
        
        // socket 보낸 후, 닫기
        send(sock, jsonStr, strlen(jsonStr), 0);
        close(sock);

        //수집 주기(2초마다 대기)
        this_thread::sleep_for(chrono::seconds(2));
    }
};

int cnt = 1;

int main(int argc, char *argv[])
{
    MetricCollector metricCollector;
    while (true)
    {
        metricCollector.runThread = thread([&](){ metricCollector.run(); });
        metricCollector.runThread.detach();

        // metricCollector.networkSpeedThread = thread([&](){ metricCollector.networkSpeed = metricCollector.metric.getNetworkSpeed(); });
        // metricCollector.networkSpeedThread.detach();
        
        // metricCollector.workingBlockCountThread = thread([&](){ metricCollector.metric.workingBlock(); });
        // metricCollector.workingBlockCountThread.detach();
        
        this_thread::sleep_for(chrono::seconds(2));
        // std::cout << "cnt: " << cnt << std::endl;
        // cnt++;
    }
    // 소켓 닫기

    return 0;
}
