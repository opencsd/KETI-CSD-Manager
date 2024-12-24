#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>#
#include <chrono>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>

#include "metric_check.h"



// 테스트용 메인문
// int main(){
//     QueryDuration querydu;
//     CPU_Usage cpu_usg;
//     result_Net_Usage net_usg;
//     Metric_Result result;

//     QueryStart(querydu, cpu_usg, net_usg);

//     std::cout << "--------main start----------" << std::endl;
//     std::cout << "cpu 사용량 : " << cpu_usg.start_cpu << std::endl;
//     std::cout << "Net 사용량 rx : " << net_usg.start_rx << std::endl;
//     std::cout << "Net 사용량 tx : " << net_usg.start_tx << std::endl;

//     sleep(5);

//     QueryEnd(querydu, cpu_usg, net_usg, result);

//     std::cout << "--------main end----------" << std::endl;
//      std::cout << "cpu 사용량 : " << cpu_usg.end_cpu << std::endl;
//     std::cout << "Net 사용량 rx : " << net_usg.end_rx << std::endl;
//     std::cout << "Net 사용량 tx : " << net_usg.end_tx << std::endl;

//     return 0;
// }

int main(int argc, char* argv[]){
    if(argc < 2){
        std::cerr << "Usage: " << argv[0] << " <integer_argument>\n";
        return 1;
    }
    
    int snippet_num = std::atoi(argv[1]);

    QueryDuration querydu;
    CPU_Usage cpu_usg;
    result_Net_Usage net_usg;
    Metric_Result result;

    // Interface와 통신으로 쿼리 시작, 종료 받음
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "Socket creation error" << std::endl;
        return -1;
    }

    // Attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        std::cerr << "Setsockopt error" << std::endl;
        return -1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    // Bind the socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) {
        std::cerr << "Bind failed" << std::endl;
        return -1;
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Listen error" << std::endl;
        return -1;
    }

    int startCount = 0;
    int endCount = 0;

    while (true)
    {
        // Accept incoming connections
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
            std::cerr << "Accept error" << std::endl;
            return -1;
        }

        // Receive message from the client
        valread = read(new_socket, buffer, 1024);
        buffer[valread] = '\0';
        if(strcmp(buffer, "Query Start") == 0){
            if(startCount == 0){
                QueryStart(querydu, cpu_usg, net_usg);
            }
            startCount++;
            // std::cout << "startCount : " << startCount << std::endl;
        } 
        else {
            // std::cout << "endCount : " << endCount << std::endl;
            endCount++;
            if(endCount == snippet_num){
                std::cout << "Metric end " << std::endl;
                QueryEnd(querydu, cpu_usg, net_usg, result);
                break;
            }
            
        }

        close(new_socket);
    }
    close(server_fd);

    send_Metric_result(result);

    std::cout << "쿼리 수행동안 CSD CPU 사용량 : " << result.cpu_usage << std::endl;
    std::cout << "쿼리 수행동안 CSD Power 사용량(W) : " << result.power_usage << std::endl;
    std::cout << "쿼리 수행동안 CSD Net 사용량(Mb) : " << result.network_usage << std::endl;

    return 0;
}
