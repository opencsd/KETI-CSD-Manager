#!/usr/bin/env bash

file_path1="/home/ngd/opencsd-storage-node8/"
file_path2="/home/ngd/opencsd-storage-node8/keti/"
password="1234"

num_of_csd=$1 # 1, 2, 3, 4, 5, 6, 7, 8
file_name=$2 # keti or file_name

if [ "$2" == "keti" ]
then
    cd /root/workspace/opencsd-storage-node8
    for((i=1;i<$num_of_csd+1;i++)); do
        ip="10.1.$i.2"
        echo scp -r $file_name ngd@$ip:$file_path1 copying...
        sshpass -p $password scp -r $file_name ngd@$ip:$file_path1
    done
else
    for((i=1;i<$num_of_csd+1;i++)); do
        ip="10.1.$i.2"
        echo scp $file_name ngd@$ip:$file_path2 copying...
        sshpass -p $password scp $file_name ngd@$ip:$file_path2
    done
fi


