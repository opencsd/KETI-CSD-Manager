
#include<stdio.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>
//#include<string.h>

#include <cstdio>
#include <string>
#include <iostream>

#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/options.h"

using namespace ROCKSDB_NAMESPACE;

std::string kDBPath = "/tmp/rocksdb_simple_example";

int rocksdbPut(char* data, size_t size) {
    
  DB* db;
  Options options;
  // Optimize RocksDB. This is the easiest way to get RocksDB to perform well
  options.IncreaseParallelism();
  options.OptimizeLevelStyleCompaction();
  // create the DB if it's not already present
  options.create_if_missing = true;

  std::cout << "check0" << std::endl;
  // open DB
  Status s = DB::Open(options, kDBPath, &db);
  assert(s.ok());
  std::cout << "check1" << std::endl;

  // Put key-value
  s = db->Put(WriteOptions(), "key1", "value");
  assert(s.ok());
  std::string value;
  // get value
  s = db->Get(ReadOptions(), "key1", &value);
  assert(s.ok());
  assert(value == "value");
  std::cout << "check2" << std::endl;

  // atomically apply a set of updates
  {
    WriteBatch batch;
    batch.Delete("key1");
    batch.Put("key2", value);
    s = db->Write(WriteOptions(), &batch);
  }

  s = db->Get(ReadOptions(), "key1", &value);
  assert(s.IsNotFound());

  db->Get(ReadOptions(), "key2", &value);
  assert(value == "value");
  std::cout << "check3" << std::endl;
  {
    PinnableSlice pinnable_val;
    db->Get(ReadOptions(), db->DefaultColumnFamily(), "key2", &pinnable_val);
    assert(pinnable_val == "value");
  }

  {
    std::string string_val;
    // If it cannot pin the value, it copies the value to its internal buffer.
    // The intenral buffer could be set during construction.
    PinnableSlice pinnable_val(&string_val);
    db->Get(ReadOptions(), db->DefaultColumnFamily(), "key2", &pinnable_val);
    assert(pinnable_val == "value");
    // If the value is not pinned, the internal buffer must have the value.
    assert(pinnable_val.IsPinned() || string_val == "value");
    std::cout << "check4" << std::endl;
  }

  PinnableSlice pinnable_val;
  s = db->Get(ReadOptions(), db->DefaultColumnFamily(), "key1", &pinnable_val);
  assert(s.IsNotFound());
  // Reset PinnableSlice after each use and before each reuse
  pinnable_val.Reset();
  db->Get(ReadOptions(), db->DefaultColumnFamily(), "key2", &pinnable_val);
  assert(pinnable_val == "value");
  pinnable_val.Reset();
  std::cout << "check5" << std::endl;
  // The Slice pointed by pinnable_val is not valid after this point

  delete db;

  return 0;
}

int main(int argc, char* argv[])
{
    int serv_sock;
    int clint_sock;

    struct sockaddr_in serv_addr;
    struct sockaddr_in clint_addr;
    socklen_t clnt_addr_size;

    if(argc != 2)
    {
        printf("%s <port>\n", argv[0]);
        exit(1);
    }
    serv_sock = socket(PF_INET, SOCK_STREAM,0); //1번
    if(serv_sock == -1)
        printf("socket error\n");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock,(struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) //2번
        printf("bind error\n");
    if(listen(serv_sock,5)==-1) //3번
        printf("listen error\n");

    while(1) {

        clnt_addr_size = sizeof(clint_addr);
        clint_sock = accept(serv_sock,(struct sockaddr*)&clint_addr,&clnt_addr_size); //4번
        if(clint_sock == -1)
                printf("accept error\n");

        printf("accept\n");
        while(1) {
                int MAX_CHAR = 100;
                char message[MAX_CHAR];
                int str_len;

                printf("wait read\n");
                if ((str_len = read(clint_sock, message, sizeof(message) )) == -1 ) { //5번
                        printf("read error");
                        exit(1);
                }
                printf("str_len = %d\n", str_len);
                printf("message = ");
                //message[str_len] = '\0';
                for (int i=0; i< str_len; i++) {
                        printf("%c", message[i]);
                }
                printf("\n");

                printf("클라이언트에서 : %s \n", message);
                rocksdbPut(message, str_len);


                if(str_len != MAX_CHAR-1)
                        break;
        }

        //char msg[] = "hello this is server!\n";
        //write(clint_sock, msg, sizeof(msg));

    }
    close(serv_sock); //6번
    close(clint_sock);
    return 0;
}

