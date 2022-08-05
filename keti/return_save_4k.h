#include <stdlib.h>
#include <sstream>
#include <tuple>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>

#include "input.h"

using namespace rapidjson;
using namespace std;

#define BUFF_SIZE 4096
#define PACKET_SIZE 36

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
    int work_id;
    string csd_name;
    int total_block_count;
    vector<tuple<int, int, int, bool>> block_id_list; 
    // block_id : block start offset : block size : is full block
    // 블록 데이터 없을땐 block start offse = -1
    vector<int> row_offset; 
    int rows;
    char data[4096];//4k = 4096 찰때마다 전송
    int length;
    int block_count;

    MergeResult(){}

    //scan.cc의 only scan 작업의 생성자 (scan.cc->returnQ)
    MergeResult(
        int work_id_, string csd_name_, int total_block_count_, 
        int block_id_, int length_, vector<int> row_offset_, 
        int rows_, char *data_
    ){
        work_id = work_id_;
        csd_name = csd_name_;
        total_block_count = total_block_count_;
        block_id_list.push_back(make_tuple(block_id_,0,length_,true));
        row_offset.assign(row_offset_.begin(), row_offset_.end());
        rows = rows_;
        memcpy(data, data_, length_);
        length = length_;
        block_count = 1;
    }

    //merge.cc의 최초 생성자
    MergeResult(int workid_, string csdname_, int total_block_count_){
        work_id = workid_;
        csd_name = csdname_;
        total_block_count = total_block_count_;
        block_id_list.clear();
        row_offset.clear();
        rows = 0;
        length = 0;
        memset(&data, 0, sizeof(BUFF_SIZE));
        block_count = 0;
    }

    void InitMergeResult(){
        this->block_id_list.clear();
        this->row_offset.clear();
        memset(&this->data, 0, sizeof(BUFF_SIZE));
        this->rows = 0;
        this->length = 0;
    }
};