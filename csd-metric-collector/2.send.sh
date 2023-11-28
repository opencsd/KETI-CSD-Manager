# 컨테이너 빌드에 필요한 파일들 csd1의 test 폴더로 전송
num_of_csd=$1 # 1, 2, 3, 4, 5, 6, 7, 8

# cd $file_path1
for((i=1;i<$num_of_csd+1;i++)); 
do
    ip="10.1.$i.2"
    scp csd-metric-collector-tcpip-aarch64 dockerfile container-processing.sh root@$ip:/home/ngd/workspace/csd-metric-collector-docker

    # sshpass -p $password scp -rp -o ConnectTimeout=60 $file_name root@$ip:$file_path2
done