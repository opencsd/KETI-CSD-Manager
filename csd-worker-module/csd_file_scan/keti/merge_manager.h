#pragma once
#include <limits.h>
#include <string>
#include <stack>
#include <bitset>
#include "return.h"

struct Result;
extern WorkQueue<Result> MergeQueue;
extern WorkQueue<MergeResult> ReturnQueue;

struct T{
  string varString;
  int varInt;
  int64_t varLong;
  float varFloat;
  double varDouble;
  int real_size;//소수점 길이//여기추가
};

typedef std::pair<int, int> pair_key;
struct pair_hash{
    template <class T1, class T2>
    std::size_t operator() (const std::pair<T1, T2> &pair) const{
        return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
    }
};

class MergeManager{
public:
    // MergeManager(){}
    void Merging();
    void MergeBlock(Result &result);
    void SendDataToBufferManager(MergeResult &mergedBlock);

private:
    unordered_map<pair_key, MergeResult, pair_hash> m_MergeManager;// key=<qid,wid>
};

struct FilterInfo{
    vector<string> table_col;//스캔테이블
    vector<int> table_offset;
    vector<int> table_offlen;
    vector<int> table_datatype;
    unordered_map<string, int> colindexmap;//col index
    // vector<string> filtered_col;//컬럼필터테이블
    // vector<int> filtered_datatype;
    // unordered_map<string, int> filtered_colindexmap;//filter후 col index
    string table_filter;//where절 정보
    vector<Projection> column_projection;//select절 정보
    vector<int> projection_datatype;//*컬럼 프로젝션 후 컬럼의 데이터타입
    vector<string> groupby_col;//goup by절 정보
    bool need_col_filtering;
    
    FilterInfo(){}
    FilterInfo(
      vector<string> table_col_, vector<int> table_offset_, 
      vector<int> table_offlen_, vector<int> table_datatype_,
      unordered_map<string, int> colindexmap_, /*vector<string> filtered_col_,
      vector<int> filtered_datatype_,*/ string table_filter_,
      vector<Projection> column_projection_, vector<int> projection_datatype_,
      vector<string> groupby_col_)
      : table_col(table_col_),
        table_offset(table_offset_),
        table_offlen(table_offlen_),
        table_datatype(table_datatype_),
        colindexmap(colindexmap_),/*
        filtered_col(filtered_col_),
        filtered_datatype(filtered_datatype_),*/
        table_filter(table_filter_),
        column_projection(column_projection_),
        projection_datatype(projection_datatype_),
        groupby_col(groupby_col_){}
};

struct Result{
    int query_id;
    int work_id;
    string csd_name;
    int total_block_count;
    FilterInfo filter_info;
    int row_count;
    int length;
    char data[BUFF_SIZE];
    vector<int> row_offset;
    int result_block_count;

    //scan, filter의 최초 생성자
    Result(
        int query_id_, int work_id_, string csd_name_,
        int total_block_count_, FilterInfo filter_info_,
        int result_block_count_ = 0)
        : query_id(query_id_),
          work_id(work_id_), 
          csd_name(csd_name_),
          total_block_count(total_block_count_),
          filter_info(filter_info_),
          result_block_count(result_block_count_){
            row_count = 0;
            length = 0;
            memset(&data, 0, sizeof(BUFF_SIZE));
            row_offset.clear();
          } 
 
  void InitResult(){
    row_count = 0;
    length = 0;
    memset(&data, 0, sizeof(BUFF_SIZE));
    row_offset.clear();
    result_block_count = 0;
  }

};
