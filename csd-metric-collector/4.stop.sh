num_of_csd=$1

# 배열을 순회하면서 각 IP에 접속
for((i=1;i<$num_of_csd+1;i++)); 
do
    ip="10.1.$i.2"
    ssh -n root@$ip "docker stop csd-metric-collector && docker rm csd-metric-collector && docker rmi csd-metric-collector" 
    # 컨테이너 실행 중지 및 컨테이너, 이미지 삭제
done