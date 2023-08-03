#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080

void QueryStart(){
    std::cout << "Query Start!" << std::endl;
}

void QueryEnd(){
    std::cout << "Query End!" << std::endl;
}

int main() {
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
                QueryStart();
            }
            startCount++;
        } 
        else if(strcmp(buffer, "Query End") == 0) {
            endCount++;
            if(endCount == 3){
                QueryEnd();
                break;
            }
        }

        close(new_socket);
    }

    close(server_fd);
    return 0;
}