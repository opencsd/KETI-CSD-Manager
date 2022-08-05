#include <limits.h>
#include "return.h"

using namespace std; 

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
    vector<string> column_filter;
    vector<vector<string>> column_projection;
    vector<string> groupby_col;
    vector<pair<int,string>> orderby_col;
    
    FilterInfo(){}
    FilterInfo(
        string table_filter_, vector<string> table_col_,
        vector<int> table_offset_, vector<int> table_offlen_,
        vector<int> table_datatype_, unordered_map<string, int> colindexmap_,
        vector<string> column_filter_, vector<vector<string>> column_projection_,
        vector<string> groupby_col_, vector<pair<int,string>> orderby_col_)
        : table_filter(table_filter_),
          table_col(table_col_),
          table_offset(table_offset_),
          table_offlen(table_offlen_),
          table_datatype(table_datatype_),
          colindexmap(colindexmap_),
          column_filter(column_filter_),
          column_projection(column_projection_),
          groupby_col(groupby_col_),
          orderby_col(orderby_col_){}
};

struct Result{
    int work_id;
    // pair<int,list<int>> block_id_list; //<is full 10 block, block list>
    int row_count;
    int length;
    char data[BUFF_SIZE];
    vector<int> row_offset;
    string csd_name;
    FilterInfo filter_info;
    int total_block_count;
    int result_block_count;
    list<PrimaryKey> primary_key_list;
    int last_valid_block_id;//이 id 보다 큰 블록은 유효하지 않음
    vector<vector<int>> row_column_offset;
    vector<string> column_name;

    //scan.cc의 최초 생성자
    Result(
        int work_id_, int total_block_count_, 
        string csd_name_, list<PrimaryKey> primary_key_list_)
        : work_id(work_id_), 
          total_block_count(total_block_count_),
          csd_name(csd_name_), 
          primary_key_list(primary_key_list_){
            // block_id_list.first = 1;
            // block_id_list.second.clear();
            result_block_count = 0;
            last_valid_block_id = INT_MAX;
          } 

    //scan.cc의 filter로 보내는 생성자(scan filter)
    //scan.cc의 merge로 보내는 생성자(index only scan)
    Result(
        int work_id_, /*pair<int,list<int>> block_id_list_,*/
        int row_count_, int length_, char* data_,
        vector<int> row_offset_, string csd_name_,
        FilterInfo filter_info_, int total_block_count_,
        int result_block_count_, int last_valid_block_id_)
        : work_id(work_id_), 
          // block_id_list(block_id_list_),
          row_count(row_count_), length(length_),
          row_offset(row_offset_),
          csd_name(csd_name_),
          filter_info(filter_info_),
          total_block_count(total_block_count_),
          result_block_count(result_block_count_),
          last_valid_block_id(last_valid_block_id_){
            memcpy(data, data_, length);
          }      

    //filter.cc의 최초 생성자
    Result(
        int work_id_, /*pair<int,list<int>> block_id_list_,*/
        int row_count_, int length_, string csd_name_, 
        FilterInfo filter_info_, int total_block_count_,
        int result_block_count_, int last_valid_block_id_)
        : work_id(work_id_), /*block_id_list(block_id_list_),*/ 
          row_count(row_count_), length(length_),
          csd_name(csd_name_), 
          filter_info(filter_info_),
          total_block_count(total_block_count_),
          result_block_count(result_block_count_),
          last_valid_block_id(last_valid_block_id_){} 
 
  void InitResult(){
    // block_id_list.second.clear();
    row_count = 0;
    length = 0;
    memset(&data, 0, sizeof(BUFF_SIZE));
    row_offset.clear();
    result_block_count = 0;
  }

};