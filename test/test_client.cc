#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#include "metric_check.h"

#define PORT 8080



int main(int argc, char const *argv[]) {
    const char* Qstart = "Query Start";
    const char* Qend = "Query End";

    send_Query_info(Qstart);


    sleep(1);

    send_Query_info(Qend);
    

    return 0;
}