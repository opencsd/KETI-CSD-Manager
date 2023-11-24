#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#include "metric_check.h"

#define PORT 12345

int main() {
    float cpu_usage = 0;
    float power_usage = 0;
    float network_usage = 0;

    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

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
    address.sin_port = htons(PORT);

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

    int csdCount = 0;
    Metric_Result result;

    while (true)
    {
        // Accept incoming connections
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
            std::cerr << "Accept error" << std::endl;
            return -1;
        }

        // Receive message from the client
        valread = recv(new_socket, &result, sizeof(result), 0);
        cpu_usage += result.cpu_usage;
        power_usage += result.power_usage;
        network_usage += static_cast<double>(result.network_usage);
        csdCount++;
        // std::cout << "CSD CPU 사용량 : " << cpu_usage << std::endl;
        // std::cout << "CSD Power 사용량 : " << power_usage << std::endl;
        // std::cout << "CSD Net 사용량 : " << network_usage << std::endl;

        if(csdCount == 8){
            std::cout << "******** 전체 CSD Metric 합산 결과 ********" << std::endl;
            std::cout << "쿼리 수행동안 CSD CPU 사용량 : " << cpu_usage << std::endl;
            std::cout << "쿼리 수행동안 CSD Power 사용량 : " << power_usage << std::endl;
            std::cout << "쿼리 수행동안 CSD Net 사용량 : " << network_usage << std::endl;
            break;
        }
        
        close(new_socket);
    }

    close(server_fd);
    return 0;
}