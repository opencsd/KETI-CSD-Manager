#pragma once
#include <stdlib.h>
#include <sstream>
#include <tuple>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <any>
#include <cstdlib>

#include "input.h"

#define BUFF_M_PORT 8111
#define BUFF_M_IP "10.0.5.120"

struct MergeResult;

extern WorkQueue<MergeResult> ReturnQueue;

class Return{
    public:
        Return(){}
        void ReturnResult();
        void SendDataToBufferManager(MergeResult &mergeResult);
};

struct message{
    long msg_type;
    char msg[2000];
};

struct MergeResult{
    int query_id;
    int work_id;
    string csd_name;
    vector<int> row_offset; 
    int row_count; 
    char data[BUFF_SIZE];
    int length;
    int total_block_count;
    int result_block_count;
    int current_block_count;
    map<string,vector<std::any>> groupby_map;

    MergeResult(){}
    //merge.cc의 최초 생성자
    MergeResult(int query_id_, int work_id_, string csd_name_, int total_block_count_)
    : query_id(query_id_),
      work_id(work_id_),
      csd_name(csd_name_),
      total_block_count(total_block_count_){
        row_offset.clear();
        row_count = 0;
        length = 0;
        memset(&data, 0, sizeof(BUFF_SIZE));
        result_block_count = 0;
        current_block_count = 0;
        groupby_map.clear();
    }

    void InitMergeResult(){
        row_offset.clear();
        memset(&data, 0, sizeof(BUFF_SIZE));
        row_count = 0;
        length = 0;
        result_block_count = 0;
        groupby_map.clear();
    }
};  
