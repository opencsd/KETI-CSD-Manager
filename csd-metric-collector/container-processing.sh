# 1.csd 메트릭 콜렉터 빌드
# aarch64-linux-gnu-g++ -o csd-metric-collector-tcpip-aarch64 csd-metric-collector-tcpip.cc -L/root/workspace/CSD-Metric-Collector/rapidjson -pthread

# 2.컨테이너 이미지 빌드(dockerfile 기반)
docker build -t csd-metric-collector .

# 3.컨테이너 실행 
docker run -d -it --privileged --name csd-metric-collector -v /proc:/metric/cpuMemUsage -v /sys/class/net/ngdtap0/statistics:/metric/networkUsage csd-metric-collector

# 4.컨테이너 실행 확인
docker ps -a | grep csd-metric-collector

# 컨테이너 CLI 접속
docker exec -it csd-metric-collector bash