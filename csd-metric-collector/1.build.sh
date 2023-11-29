#!/usr/bin/env bash
#g++ or cross complie
# if [ "$1" == "1" ]; then
#     g++ -o csd-metric-collector-tcpip csd-metric-collector-tcpip.cc -L/root/workspace/CSD-Metric-Collector/rapidjson -pthread
# elif [ "$1" == "2" ]; then
#     aarch64-linux-gnu-g++ -o csd-metric-collector-tcpip-aarch64 csd-metric-collector-tcpip.cc -L/root/workspace/CSD-Metric-Collector/rapidjson -pthread
# else
#     echo "Invalid argument. Please provide 1 or 2 as the first argument."
# fi

# 아키텍쳐에 맞는 컴파일러인지 확인 필수
aarch64-linux-gnu-g++ -o csd-metric-collector-tcpip-aarch64 csd-metric-collector-tcpip.cc -L/root/workspace/CSD-Metric-Collector/rapidjson -pthread