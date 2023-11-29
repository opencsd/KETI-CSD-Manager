FROM ubuntu:18.04

#csd_ip => 환경변수 설정
ARG csd_ip 
ENV CSD_IP=$csd_ip

# 실행 파일을 컨테이너 내부로 복사
COPY csd-metric-collector-tcpip-aarch64 /csd-metric-collector-tcpip-aarch64