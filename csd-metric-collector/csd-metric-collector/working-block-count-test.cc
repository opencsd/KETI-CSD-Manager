#include <iostream>
#include <cstdlib>
#include <ctime>
#include <random>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../ip_config.h"

//#define SERVER_IP "10.0.4.83"
//#define SERVER_PORT 40305

#define CSD_WORKING_BLOCK_IP "10.0.4.83"
#define CSD_WORKING_BLOCK_PORT 40305

int main()
{
    // 난수 생성기 초기화
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1, 100);

    // 서버 주소 설정
    struct sockaddr_in serverAddress
    {
    };
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(CSD_WORKING_BLOCK_PORT);
    inet_pton(AF_INET, CSD_WORKING_BLOCK_IP, &(serverAddress.sin_addr));

    // 무작위 수 생성 및 서버로 전송
    while (true)
    {

        // 소켓 생성
        int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == -1)
        {
            std::cerr << "Failed to create socket" << std::endl;        }
        // 서버에 연결
        if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
        {
            std::cerr << "Failed to connect to server" << std::endl;
        }

        int randomNumber = dis(gen);
        std::cout << "Sending random number: " << randomNumber << std::endl;

        send(clientSocket, &randomNumber, sizeof(randomNumber), 0);
        std::cout << "send" << std::endl;

        // 잠시 대기
        sleep(2);
        close(clientSocket);
    }

    // 소켓 닫기

    return 0;
}