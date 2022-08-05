#include <iostream>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <mutex>
#include <queue>
#include <list>
#include <algorithm>
#include <condition_variable>
#include <unordered_map>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <stack>
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

#include "rocksdb/sst_file_reader.h"

using namespace rapidjson;
using namespace std;
using namespace ROCKSDB_NAMESPACE;

#define NUM_OF_BLOCKS 15
#define BUFF_SIZE (NUM_OF_BLOCKS * 4096)
#define INPUT_IF_PORT 8080
#define PACKET_SIZE 36

struct Snippet;
template <typename T>
class WorkQueue;

extern WorkQueue<Snippet> ScanQueue;

typedef struct Data{
  int type;
  vector<string> str_col;
  vector<int> int_col;
  vector<float> flt_col;
}Data;

/*     
                            Scan Type
  +-----------------------+--------------------------------------+
  | Full_Scan_Filter   | full table scan / only scan (no filter) |
  | Full_Scan          | full table scan / scan and filter       |
  | Index_Scan_Filter  | index pk scan   / only scan (no filter) |
  | Index_Scan         | index pk scan   / scan and filter       |
  +------------------------+-------------------------------------+
*/

enum Scan_Type{
  Full_Scan_Filter,
  Full_Scan,
  Index_Scan_Filter,
  Index_Scan
};

struct PrimaryKey{
  string key_name;
  int key_type;
  int key_length;
};

class Input{
    public:
        Input(){}
        void InputSnippet();
        void EnQueueScan(Snippet snippet_);
};

struct Snippet{
    int work_id;
    int query_id;
    string csd_name;
    string table_name;
    list<BlockInfo> block_info_list;
    vector<string> table_col;
    string table_filter;
    vector<int> table_offset;
    vector<int> table_offlen;
    vector<int> table_datatype;
    int total_block_count;
    unordered_map<string, int> colindexmap;
    list<PrimaryKey> primary_key_list;
    uint64_t kNumInternalBytes;
    int primary_length;
    char index_num[4];
    string index_pk;
    int scan_type;
    vector<string> column_filter;
    vector<vector<string>> column_projection;
    vector<string> groupby_col;
    vector<pair<int,string>> orderby_col;

    Snippet(const char* json_){
        // cout << "\n[Parsing Snippet]" << endl;	
        bool index_scan = false;
        bool only_scan = true;

        total_block_count = 0;
        Document document;
        document.Parse(json_);
        
        query_id = document["queryID"].GetInt();
        work_id = document["workID"].GetInt();
        csd_name = document["CSDName"].GetString();
        table_name = document["tableName"].GetString();

        Value &blockInfo = document["blockInfo"];
        int block_id_ = blockInfo["blockID"].GetInt(); 
        int block_list_size = blockInfo["blockList"].Size();

        int index_num_ = document["indexNum"].GetInt();
        union{
          int value;
          char byte[4];
        }inum;
        inum.value = index_num_;
        memcpy(index_num,inum.byte,4);

        for(int i = 0; i < document["columnFiltering"].Size(); i++){
          column_filter.push_back(document["columnFiltering"][i].GetString());
        }

        for(int i = 0; i < document["columnProjection"].Size(); i++){
          for(int j = 0; j < document["columnProjection"][i].Size(); j++){
            column_projection[i].push_back(document["columnProjection"][i][j].GetString());
          }
        }

        if(document.HasMember("groupBy")){
          for(int i = 0; i < document["groupBy"].Size(); i++){
            groupby_col.push_back(document["groupBy"][i].GetString());
          }
        }else{
          groupby_col.clear();
        }

        if(document.HasMember("orderBy")){
          for(int i = 0; i < document["orderBy"].Size(); i++){
            string op1 = document["orderBy"][i][0].GetString();
            string op2 = document["orderBy"][i][1].GetString();
            orderby_col.push_back(pair<int,string>(stoi(op1),op2));
          }
        }else{
          orderby_col.clear();
        }

        int primary_count = document["pk_len"].GetInt();

        if(primary_count == 0){
          kNumInternalBytes = 8;
        }else{
          kNumInternalBytes = 0;
        }

        uint64_t block_offset_, block_length_;
        
        //block list 정보 저장
        for(int i = 0; i<block_list_size; i++){
            block_offset_ = blockInfo["BlockList"][i]["Offset"].GetInt64();
            for(int j = 0; j < blockInfo["BlockList"][i]["Length"].Size(); j++){
                block_length_ = blockInfo["BlockList"][i]["Length"][j].GetInt64();
                BlockInfo newBlock(block_id_, block_offset_, block_length_);
                block_info_list.push_back(newBlock);
                block_offset_ = block_offset_ + block_length_;
                block_id_++;
                total_block_count++;
            }
        }   

        primary_key_list.clear();
        primary_length = 0;

        //테이블 스키마 정보 저장
        Value &table_col_ = document["table_col"];
        for(int j=0; j<table_col_.Size(); j++){
            string col = table_col_[j].GetString();
            int startoff = document["table_offset"][j].GetInt();
            int offlen = document["table_offlen"][j].GetInt();
            int datatype = document["table_datatype"][j].GetInt();
            table_col.push_back(col);
            table_offset.push_back(startoff);
            table_offlen.push_back(offlen);
            table_datatype.push_back(datatype);
            colindexmap.insert({col,j});
            
            //pk 정보 저장
            if(j<primary_count){
              string key_name_ = document["table_col"][j].GetString();
              int key_type_ = document["table_datatype"][j].GetInt();
              int key_length_ = document["table_offlen"][j].GetInt();
              primary_key_list.push_back(PrimaryKey{key_name_,key_type_,key_length_});
              primary_length += key_length_;
            }
        }

        //filter 작업인 경우 확인
        if(document.HasMember("table_filter")){
          Value &table_filter_ = document["table_filter"];
          Document small_document;
          small_document.SetObject();
          rapidjson::Document::AllocatorType& allocator = small_document.GetAllocator();
          small_document.AddMember("table_filter",table_filter_,allocator);
          StringBuffer strbuf;
          rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(strbuf);
	        small_document.Accept(writer);
          table_filter = strbuf.GetString();
          only_scan = false;
        }else{
          cout << "+++ only scan!! +++" << endl;//나중에 지워도 됨
        }

        //index scan인 경우 확인
        if(document.HasMember("index_pk")){
          cout << "+++ index scan!! +++" << endl;
          Value &index_pk_ = document["index_pk"];
          Document small_document;
          small_document.SetObject();
          rapidjson::Document::AllocatorType& allocator = small_document.GetAllocator();
          small_document.AddMember("index_pk",index_pk_,allocator);
          StringBuffer strbuf;
          rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(strbuf);
	        small_document.Accept(writer);
          index_pk = strbuf.GetString();
          index_scan = true;
        }else{
          cout << "no index pk" << endl;//나중에 지워도 됨
        }

        // //pk가 있을 때 pk가 필요 컬럼인지 확인
        // bool need_pk = false;
        // std::list<PrimaryKey>::iterator iter;
        // for(iter = primary_key_list.begin(); iter != primary_key_list.end(); iter++){
        //   if (*find(column_filter.begin(), column_filter.end(), (*iter).key_name) == (*iter).key_name) {
        //       need_pk = true;
        //       break;
        //   } 
        // }

        //스캔 타입 결정
        if(!index_scan && !only_scan){
          scan_type = Full_Scan_Filter;
        }else if(!index_scan && only_scan){
          scan_type = Full_Scan;
        }else if(index_scan && !only_scan){
          scan_type = Index_Scan_Filter;
        }else{
          scan_type = Index_Scan;
        }
       
    }
};

template <typename T>
class WorkQueue
{
  condition_variable work_available;
  mutex work_mutex;
  queue<T> work;

public:
  void push_work(T item){
    unique_lock<mutex> lock(work_mutex);

    bool was_empty = work.empty();
    work.push(item);

    lock.unlock();

    if (was_empty){
      work_available.notify_one();
    }    
  }

  T wait_and_pop(){
    unique_lock<mutex> lock(work_mutex);
    while (work.empty()){
      work_available.wait(lock);
    }

    T tmp = work.front();
    work.pop();
    return tmp;
  }

  int length(){
	  int ret;
	  unique_lock<mutex> lock(work_mutex);

    ret = work.size();

    lock.unlock();
	  return ret;
  }
};

    // int level_ = 0;
    // bool blocks_maybe_compressed_ = true;
    // bool blocks_definitely_zstd_compressed_ = false;
    // const bool immortal_table_ = false;
    // Slice cf_name_for_tracing_ = nullptr;
    // uint64_t sst_number_for_tracing_ = 0;
    // shared_ptr<Cache> to_block_cache_ = nullptr;//to_ = table_options
    // uint32_t to_read_amp_bytes_per_bit = 0;
    // //std::shared_ptr<const FilterPolicy> to_filter_policy = nullptr;
    // //std::shared_ptr<Cache> to_block_cache_compressed = nullptr;
    // bool to_cache_index_and_filter_blocks_ = false;
    // //ioptions
    // uint32_t footer_format_version_ = 5;//footer
    // int footer_checksum_type_ = 1;
    // uint8_t footer_block_trailer_size_ = 5;
    
    // //std::string dev_name = "/usr/local/rocksdb/000051.sst";
    // /*block info*/

    // std::string dev_name = "/dev/sda";
    // // const uint64_t handle_offset = 43673280512;
    // // const uint64_t block_size = 3995; //블록 사이즈 틀리지 않게!!