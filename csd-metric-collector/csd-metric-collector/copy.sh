file_path1="/root/workspace/CSD-Metric-Collector/csd-metric-collector-tcpip"
file_path2="/home/ngd/workspace/csd-metric-collector/csd-metric-collector-tcpip"
password="1234"

num_of_csd=$1 # 1, 2, 3, 4, 5, 6, 7, 8
file_name=$2 # csd-metric-collector-tcpip-aarch64

# cd $file_path1
for((i=1;i<$num_of_csd+1;i++)); do
    ip="10.1.$i.2"
    echo scp -rp $file_name root@$ip:$file_path1 copying...
    sshpass -p $password scp -rp -o ConnectTimeout=60 $file_name root@$ip:$file_path2
done

