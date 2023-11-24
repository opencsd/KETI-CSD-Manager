#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <unordered_map>

// #define PORT 8080


struct QueryDuration {
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point end_time;
};

struct CPU_Usage {
    unsigned long long start_total;
    unsigned long long end_total;
    unsigned long long start_idle;
    unsigned long long end_idle;
    double cpu_usage_percentage;
};

struct temp_CPU {
    unsigned long long total;
    unsigned long long idle;
};

struct Net_Usage {
    unsigned long long rx;
    unsigned long long tx;
};

struct result_Net_Usage {
    unsigned long long start_rx;
    unsigned long long start_tx;
    unsigned long long end_rx;
    unsigned long long end_tx;
};

struct Metric_Result {
    double cpu_usage;
    float power_usage;
    double network_usage;
};

// CPU 총 사용량 구하는 함수
void get_cpu_usage(temp_CPU& temp) {
    std::ifstream proc_stat("/proc/stat");
    std::string line;
    std::getline(proc_stat, line); // 첫 번째 라인은 'cpu' 정보
    std::istringstream iss(line);
    std::string cpuLabel;
    iss >> cpuLabel; // 'cpu' 레이블 버리기

    // 'cpu' 이후의 값들을 읽어와서 사용량 계산
    long user, nice, sys, idle, iowait, irq, softirq, steal, guest, guest_nice;
    iss >> user >> nice >> sys >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;

    proc_stat.close();

    // 현재 시점의 CPU total time 계산
    unsigned long long total_time = 0;
    total_time = user + nice + sys + idle + iowait + irq + softirq + steal + guest + guest_nice;
    temp.total = total_time;

    unsigned long long total_idle = 0;
    total_idle = idle +iowait;
    temp.idle = total_idle;

    // std::cout << "total_cpu : " << total_time << std::endl;
    // std::cout << "total_idle : " << total_idle << std::endl;
}

// 쿼리 시간 동안 CPU 평균 사용량 구하는 함수
void get_CPU_Usage_Percentage(CPU_Usage& cpu_usage, Metric_Result& result) {
    unsigned long long total_cpu = cpu_usage.end_total - cpu_usage.start_total;
    if(total_cpu <= 0) {
        printf("get CPU Usage error!");
    }
    unsigned long long total_idle = cpu_usage.end_idle - cpu_usage.start_idle;
    double cpu_usage_percentage = static_cast<double>(total_cpu - total_idle) / total_cpu * 100.00;
    double cpu_usg = static_cast<double>(total_cpu - total_idle);

    // std::cout << "get_CPU_Usage_Percentage : " << cpu_usage_percentage << std::endl;
    
    cpu_usage.cpu_usage_percentage = cpu_usage_percentage;
    result.cpu_usage = cpu_usg;
}

// Network 사용량 
void get_Network_Usage(Net_Usage& net_usg) {
    // Net_Usage net_usg;
    unsigned long long rxByte;
    unsigned long long txByte;
    std::ifstream file("/proc/net/dev");
    if (!file) {
        std::cerr << "Failed to open /proc/net/dev" << std::endl;
    }

    std::string line;
    std::string name;

    while (getline(file, line)){
        if(line.find("ngdtap0") != std::string::npos){
            std::istringstream iss(line);
            iss >> name >> rxByte;
            for (int i = 3; i < 11; i++){
                iss >> txByte;
            }
            break;
        }
    }

    net_usg.rx = rxByte;
    net_usg.tx = txByte;

    // std::cout << "get_Network_Usage : " << rxByte << "   " << txByte << std::endl;
}

void QueryStart(QueryDuration& querydu, CPU_Usage& cpu_usg, result_Net_Usage& net_usg){
    // 쿼리 시작 시점 저장
    querydu.start_time = std::chrono::high_resolution_clock::now();
    
    // 쿼리 시작 시점 cpu 사용량 저장
    temp_CPU temp_cpu;
    get_cpu_usage(temp_cpu);
    cpu_usg.start_total = temp_cpu.total;
    cpu_usg.start_idle = temp_cpu.idle;

    //쿼리 시작 시점 net 값 저장
    Net_Usage temp_net;
    get_Network_Usage(temp_net);
    net_usg.start_rx = temp_net.rx;
    net_usg.start_tx = temp_net.tx;

    // std::cout << "in Query Start" << std::endl;
    // std::cout << "cpu 사용량 : " << cpu_usg.start_cpu << std::endl;
    // std::cout << "Net 사용량 rx : " << net_usg.start_rx << std::endl;
    // std::cout << "Net 사용량 tx : " << net_usg.start_tx << std::endl;
}

void QueryEnd(QueryDuration& querydu, CPU_Usage& cpu_usg, result_Net_Usage& net_usg, Metric_Result& result){
    //쿼리 끝 시점 저장
    querydu.end_time = std::chrono::high_resolution_clock::now();
    
    //쿼리 끝 시점 cpu 사용량 저장
    temp_CPU temp_cpu;
    get_cpu_usage(temp_cpu);
    cpu_usg.end_total = temp_cpu.total;
    cpu_usg.end_idle = temp_cpu.idle;

    //쿼리 수행 시간 동안의 cpu 사용량 계산
    get_CPU_Usage_Percentage(cpu_usg, result);
    if(result.cpu_usage < 0){
        printf("CPU_Percentage error!\n");
    }

    //쿼리 끝 시점 net 값 저장
    Net_Usage temp_net;
    get_Network_Usage(temp_net);
    net_usg.end_rx = temp_net.rx;
    net_usg.end_tx = temp_net.tx;

    //쿼리 수행 시간 동안의 net 계산
    unsigned long long rxByte_usage, txByte_usage, totalByte;
    double result_byte;
    rxByte_usage = net_usg.end_rx - net_usg.start_rx;
    txByte_usage = net_usg.end_tx - net_usg.start_tx;
    totalByte = rxByte_usage + txByte_usage;
    // std::cout << totalByte << std::endl;

    result_byte = static_cast<double>(totalByte) / 1048576.0;
    
    // std::cout << "Network Usage : " << result_byte << std::endl;
    
    result.network_usage = result_byte;


    //쿼리 수행 시간 동안의 power 계산
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(querydu.end_time - querydu.start_time);
    float power_usg = 12 * cpu_usg.cpu_usage_percentage * (duration.count() / 1000.0) / 100.0;
    result.power_usage = power_usg;
    // std::cout << "power 계산 결과 : " << power_usg << std::endl;

    // std::cout << "in Query End" << std::endl;
    // std::cout << "cpu 사용량 : " << cpu_usg.end_cpu << std::endl;
    // std::cout << "Net 사용량 rx : " << net_usg.end_rx << std::endl;
    // std::cout << "Net 사용량 tx : " << net_usg.end_tx << std::endl;
    // std::cout << "쿼리 수행동안 CPU 사용량(%) : " << result.cpu_usage << std::endl;
    // std::cout << "쿼리 수행동안 Power 사용량(W) : " << result.power_usage << std::endl;
    // std::cout << "쿼리 수행동안 Net 사용량(mb) : " << result.network_usage << std::endl;
}

//Query Start, End 메세지 전송
// void send_Query_info(const char* message) {
//     int sock = 0, valread;
//     struct sockaddr_in serv_addr;

//     // Create socket file descriptor
//     if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
//         std::cerr << "Socket creation error" << std::endl;
//         // return -1;
//     }

//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(PORT);

//     // Convert IPv4 and IPv6 addresses from text to binary form
//     if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
//         std::cerr << "Invalid address/ Address not supported" << std::endl;
//         // return -1;
//     }

//     // Connect to the server
//     if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
//         std::cerr << "Connection Failed" << std::endl;
//         // return -1;
//     }

//     // Send message to the server
//     send(sock, message, strlen(message), 0);
//     std::cout << "Query info sent" << std::endl;

//     close(sock);
// }

//Metric 측정 결과 host에 전송
void send_Metric_result(Metric_Result& result) {
    int sock = 0, valread;
    struct sockaddr_in serv_addr;

    const char* serverIP = "10.0.4.83";
    const int serverPort = 12345;

    // Create socket file descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        // return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serverPort);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, serverIP, &serv_addr.sin_addr)<=0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        // return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        // return -1;
    }

    // Send message to the server
    send(sock, &result, sizeof(result), 0);
    std::cout << "Metric Result sent to Host." << std::endl;

    close(sock);
}