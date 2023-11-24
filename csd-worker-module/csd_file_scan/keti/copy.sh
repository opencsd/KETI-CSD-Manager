#!/usr/bin/env bash

file_path="/home/ngd/csd-worker-module/csd_test_file/keti"
password="1234"

num_of_csd=$1 # 1, 2, 3, 4, 5, 6, 7, 8
file_name=$2 # file_name

for((i=1;i<$num_of_csd+1;i++)); do
    ip="10.1.$i.2"
    echo scp -p $file_name root@$ip:$file_path copying...
    sshpass -p $password scp -p $file_name root@$ip:$file_path
done



