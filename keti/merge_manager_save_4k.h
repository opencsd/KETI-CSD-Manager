#include "return.h"

using namespace std;

#define BUFF_SIZE 4096
#define PACKET_SIZE 36

struct Result;

extern WorkQueue<Result> MergeQueue;
extern WorkQueue<MergeResult> ReturnQueue;

class MergeManager{
public:
    MergeManager(){}
    void Merging();
    void MergeBlock(Result &result);
    void SendDataToBufferManager(MergeResult &mergedBlock);

private:
    unordered_map<int, MergeResult> m_MergeManager;// key=workid
};

struct FilterInfo{
    string table_filter;
    vector<string> table_col;
    vector<int> table_offset;
    vector<int> table_offlen;
    vector<int> table_datatype;
    unordered_map<string, int> colindexmap;

    FilterInfo(){}
    FilterInfo(
        string table_filter_, vector<string> table_col_,
        vector<int> table_offset_, vector<int> table_offlen_,
        vector<int> table_datatype_, unordered_map<string, int> colindexmap_)
        : table_filter(table_filter_),
          table_col(table_col_),
          table_offset(table_offset_),
          table_offlen(table_offlen_),
          table_datatype(table_datatype_),
          colindexmap(colindexmap_){}
};

struct Result{
    int work_id;
    int block_id;
    int total_block_count;
    int row_count;
    int length;
    char data[4096];
    vector<int> row_offset;
    string csd_name;
    FilterInfo filter_info;

    Result(){}

    //scan.cc의 filter로 보내는 생성자
    Result(
        int work_id_, int block_id_, int total_block_count_,
        int row_count_, int length_, char* data_,
        vector<int> row_offset_, string csd_name_,
        FilterInfo filter_info_ )
        : work_id(work_id_), block_id(block_id_), 
          total_block_count(total_block_count_),
          row_count(row_count_), length(length_),
          row_offset(row_offset_),
          csd_name(csd_name_),
          filter_info(filter_info_){
            memcpy(data, data_, length);
          } 

    //filter.cc의 최초 생성자
    Result(
        int work_id_, int block_id_, int total_block_count_,
        int row_count_, int length_, string csd_name_)
        : work_id(work_id_), block_id(block_id_), 
          total_block_count(total_block_count_),
          row_count(row_count_), length(length_),
          csd_name(csd_name_){} 
 
};