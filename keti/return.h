#include <stdlib.h>
#include <sstream>
#include <tuple>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>

#include "input.h"

using namespace rapidjson;
using namespace std;

#define BUFF_M_PORT 8888

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
    // vector<pair<int,list<int>>> merged_block_id_list; 
    // <is full 10 block, block id list>
    vector<int> row_offset; 
    int rows; 
    char data[BUFF_SIZE];
    int length;
    int total_block_count;
    int result_block_count;
    int current_block_count;
    int last_valid_block_id;//전체가 valid라면 == total_block_count일것
    map<string,vector<Data>> groupby_map;

    //merge.cc의 최초 생성자
    MergeResult(int workid_, string csdname_, int total_block_count_){
        work_id = workid_;
        csd_name = csdname_;
        // merged_block_id_list.clear();
        row_offset.clear();
        rows = 0;
        length = 0;
        memset(&data, 0, sizeof(BUFF_SIZE));
        total_block_count = total_block_count_;
        result_block_count = 0;
        current_block_count = 0;
        last_valid_block_id = INT_MAX;
    }

    void InitMergeResult(){
        // merged_block_id_list.clear();
        row_offset.clear();
        memset(&data, 0, sizeof(BUFF_SIZE));
        rows = 0;
        length = 0;
        result_block_count = 0;
    }
};