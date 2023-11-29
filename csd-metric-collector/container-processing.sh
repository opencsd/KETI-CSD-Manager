# 컨테이너 실행 중지 및 컨테이너, 이미지 삭제
docker stop csd-metric-collector
docker rm csd-metric-collector
docker rmi csd-metric-collector

# 컨테이너 이미지 빌드시 csd ip 환경 변수로 부여
interface_name="ngdtap0"
interface_info=$(ifconfig "$interface_name" 2>/dev/null)
if [ $? -eq 0 ]; then
    inet_address=$(echo "$interface_info" | grep -oP 'inet\s+\K[0-9.]+') # csd ip 추출 

    if [ -n "$inet_address" ]; then
        echo "IP Address of $interface_name: $inet_address"
    else
        echo "No IP address found for $interface_name"
    fi
else
    echo "Error: Unable to retrieve information for $interface_name"
fi

# 2.컨테이너 이미지 빌드(dockerfile 기반)
docker build -t csd-metric-collector --build-arg csd_ip=$inet_address .

# 3.컨테이너 실행 
docker run -d -it --privileged --name csd-metric-collector -v /proc:/metric/cpuMemUsage -v /sys/class/net/ngdtap0/statistics:/metric/networkUsage csd-metric-collector 

command_to_run="./csd-metric-collector-tcpip-aarch64"

# 4.metric collector 실행
docker exec -d csd-metric-collector $command_to_run

# # 4.컨테이너 실행 확인
# docker ps -a | grep csd-metric-collector