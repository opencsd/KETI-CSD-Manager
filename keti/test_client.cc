#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#define PORT 8080

void send_Query_info(const char* message) {
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    // Create socket file descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        // return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        // return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        // return -1;
    }

    // Send message to the server
    send(sock, message, strlen(message), 0);
    std::cout << "Query info sent" << std::endl;

    // Receive message from the server
    valread = read(sock, buffer, 1024);
    std::cout << buffer << std::endl;
}

int main(int argc, char const *argv[]) {
    const char* Qstart = "Query Start";
    const char* Qend = "Query End";

    send_Query_info(Qstart);


    sleep(5);

    send_Query_info(Qend);
    

    return 0;
}