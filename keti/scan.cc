// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory)
#include "scan.h"

uint64_t kNumInternalBytes_;
const int indexnum_size = 4;
bool index_valid;
bool first_row;
int ipk;
char sep = 0x03;
char gap = 0x20;
char fin = 0x02;

// int scanrownum = 0;
// int bfscanrow = 0;
// int totalrownum = 0;

int getPrimaryKeyData(const char* ikey_data, char* dest, list<PrimaryKey> pk_list);

inline Slice ExtractUserKey(const Slice& internal_key) {
  assert(internal_key.size() >= kNumInternalBytes_);
  return Slice(internal_key.data(), internal_key.size() - kNumInternalBytes_);
}

class InternalKey {
 private:
  string rep_;

 public:
  InternalKey() {}  // Leave rep_ as empty to indicate it is invalid
  void DecodeFrom(const Slice& s) { rep_.assign(s.data(), s.size()); }
  Slice user_key() const { return ExtractUserKey(rep_); }
};

void Scan::Scanning(){
  //cout << "<-----------  Scan Layer Running...  ----------->\n";

    while (1){
        Snippet snippet = ScanQueue.wait_and_pop();

        // cout << "\n[Get Parsed Snippet] \n #Snippet Work ID: " << snippet.work_id << "" << endl;	  

        TableRep table_rep = CSDTableManager_.GetTableRep(snippet.table_name);
        kNumInternalBytes_ = snippet.kNumInternalBytes;
		                
        Options options;
        SstBlockReader sstBlockReader(
            options, table_rep.blocks_maybe_compressed, table_rep.blocks_definitely_zstd_compressed, 
            table_rep.immortal_table, table_rep.read_amp_bytes_per_bit, table_rep.dev_name);

        list<BlockInfo>::iterator iter;
        Result scanResult(snippet.work_id, snippet.total_block_count, snippet.csd_name, snippet.primary_key_list);
        
        int current_block_count = 0;
        index_valid = true;
        ipk = 0;

        for(iter = snippet.block_info_list.begin(); iter != snippet.block_info_list.end(); iter++){//블록
            // printf("(id:%d|offset:%ld|size:%ld)\n",iter->block_id, iter->block_offset, iter->block_size);

            current_block_count++;
            scanResult.result_block_count++;

            // cout << "****block_count: " << current_block_count << " | total_block: "  << snippet.total_block_count << endl;
            BlockInfo blockInfo = *iter;
            first_row = true;
            BlockScan(&sstBlockReader, &blockInfo, &snippet, &scanResult);

            if(!index_valid){
              if(first_row){
                  //블록의 첫번째 row부터 invalid인 경우 블록 수 카운트가 잘 되는지 확인할 필요 있음
                  scanResult.last_valid_block_id = blockInfo.block_id - 1;
                  cout << "<invalid> first row / " << scanResult.last_valid_block_id << "/" << scanResult.result_block_count << endl;
              }else{
                  // scanResult.block_id_list.second.push_back(blockInfo.block_id);
                  scanResult.last_valid_block_id = blockInfo.block_id;
                  cout << "<invalid> not first row / " << scanResult.last_valid_block_id << "/" << scanResult.result_block_count << endl;
              }
              scanResult.result_block_count += snippet.total_block_count - current_block_count;
              current_block_count = snippet.total_block_count;
            }else{
              // scanResult.block_id_list.second.push_back(blockInfo.block_id);
            }

            if(current_block_count == snippet.total_block_count){
              EnQueueData(scanResult, snippet);
              scanResult.InitResult();
              // cout << "[Scan done]" << endl;
              // cout << "block_count: " << current_block_count << " | total_block: " 
              // << snippet.total_block_count << endl;
              break;
            }else if(current_block_count % NUM_OF_BLOCKS == 0){
              // if(current_block_count == 15){
              //   cout << "scan_result.length: " << scanResult.length << endl;
              //   cout << "\n----------------------------------------------\n";
              //   for(int i = 0; i < scanResult.length; i++){
              //       printf("%02X",(u_char)scanResult.data[i]);
              //   }
              //   cout << "\n------------------------------------------------\n";
              // }
              // cout << "[Enqueue Data / Init Result]" << endl;
              // cout << "block_count: " << current_block_count << " | total_block: " 
              // << snippet.total_block_count << endl;
              EnQueueData(scanResult, snippet);
              scanResult.InitResult();
            }
        }
    }
}

void Scan::BlockScan(SstBlockReader* sstBlockReader_, BlockInfo* blockInfo, Snippet *snippet_, Result *scan_result){

  Status s  = sstBlockReader_->Open(blockInfo);
  if(!s.ok()){
      cout << "open error" << endl;
  }

  const char* ikey_data;
  const char* row_data;
  size_t row_size;

  Iterator* datablock_iter = sstBlockReader_->NewIterator(ReadOptions());

  if(snippet_->scan_type == Full_Scan_Filter || snippet_->scan_type == Full_Scan){//full table scan

    for (datablock_iter->SeekToFirst(); datablock_iter->Valid(); datablock_iter->Next()) {//iterator first부터 순회
      scan_result->row_offset.push_back(scan_result->length);

      Status s = datablock_iter->status();
      if (!s.ok()) {
        cout << "Error reading the block - Skipped \n";
        break;
      }               

      const Slice& key = datablock_iter->key();
      const Slice& value = datablock_iter->value();

      InternalKey ikey;
      ikey.DecodeFrom(key);

      ikey_data = ikey.user_key().data();
      row_data = value.data();
      row_size = value.size();
      
      // std::cout << "key: " << key.ToString(true) << " | ikey: " << ikey.user_key().ToString(true) << endl;//체크해보기

      //check row index number
      char index_num[indexnum_size];
      memcpy(index_num,ikey_data,indexnum_size);
      if(memcmp(snippet_->index_num, index_num, indexnum_size) != 0){//출력 지우지 말기
        cout << "different index number: ";
        for(int i=0; i<indexnum_size; i++){
          printf("(%02X %02X)",(u_char)snippet_->index_num[i],(u_char)index_num[i]);
        }
        cout << endl;
        index_valid = false;
        return;
      }

      first_row = false;//블록의 첫번째 row부터 invalid인지 여부

      // std::cout << "[Row(HEX)] KEY: " << ikey.user_key().ToString(true) << " | VALUE: " << value.ToString(true) << endl;

      if(snippet_->primary_length != 0){ //pk있으면 붙이기
        char total_row_data[snippet_->primary_length+row_size];
        int pk_length;

        pk_length = getPrimaryKeyData(ikey_data, total_row_data, snippet_->primary_key_list);//key
       
        memcpy(total_row_data + pk_length, row_data, row_size);//key+value
        memcpy(scan_result->data + scan_result->length, total_row_data, pk_length + row_size);//buff+key+value
        
        scan_result->length += row_size + pk_length;
        scan_result->row_count++;
      }else{//없으면 value만 붙이기
          memcpy(scan_result->data+scan_result->length, row_data, row_size);
          scan_result->length += row_size;
          scan_result->row_count++;
      }

      // scanrownum = scan_result->row_count;
      // if(scanrownum < bfscanrow){
      //   totalrownum += bfscanrow;
      // }
      // bfscanrow = scanrownum;
      // cout << totalrownum << endl;
    } 

  }else{//index scan
    string pk_str = snippet_->index_pk;
    Document document;
    document.Parse(pk_str.c_str());
    Value &index_pk = document["index_pk"];

    vector<char> target_pk;
    target_pk.assign(snippet_->index_num,snippet_->index_num+4);

    bool pk_valid = true;

    while(pk_valid){
      for(int i=0; i<index_pk.Size(); i++){
        int key_type = snippet_->table_datatype[i];

        if(key_type == 3 || key_type == 14){//int(int, date)
          int key_length = snippet_->table_offlen[i];
          union{
            int value;
            char byte[4];
          }pk;
          pk.value = index_pk[i][ipk].GetInt();
          for(int j=0; j<key_length; j++){
            target_pk.push_back(pk.byte[j]);
          }

        }else if(key_type == 8 || key_type == 246){//int64_t(bigint, decimal)
          int key_length = snippet_->table_offlen[i];
          union{
            int64_t value;
            char byte[8];
          }pk;
          pk.value = index_pk[i][ipk].GetInt64();
          for(int j=0; j<key_length; j++){
            target_pk.push_back(pk.byte[j]);
          }

        }else if(key_type == 254 || key_type == 15){//string(string, varstring)
          string pk = index_pk[i][ipk].GetString();
          int key_length = pk.length();
          for(int j=0; j<key_length; j++){
            target_pk.push_back(pk[j]);
          }
        }
      }

      char *p = &*target_pk.begin();
      Slice target_slice(p,target_pk.size());

      datablock_iter->Seek(target_slice);

      if(datablock_iter->Valid()){//target과 pk가 같은지 한번더 비교
        const Slice& key = datablock_iter->key();
        const Slice& value = datablock_iter->value();

        InternalKey ikey;
        ikey.DecodeFrom(key);
        ikey_data = ikey.user_key().data();

        //테스트 출력
        std::cout << "[Row(HEX)] KEY: " << ikey.user_key().ToString(true) << " | VALUE: " << value.ToString(true) << endl;

        if(target_slice.compare(ikey.user_key()) == 0){ //target O
          row_data = value.data();
          row_size = value.size();

          char total_row_data[snippet_->primary_length+row_size];
          int pk_length;

          pk_length = getPrimaryKeyData(ikey_data, total_row_data, snippet_->primary_key_list);//key
        
          memcpy(total_row_data + pk_length, row_data, row_size);//key+value
          memcpy(scan_result->data + scan_result->length, total_row_data, pk_length + row_size);//buff+key+value
          
          scan_result->length += row_size + pk_length;
          scan_result->row_count++;
          
        }else{//target X
          //primary key error(출력 지우지 말기)
          cout << "primary key error [no primary key!]" << endl;      

          cout << "target: ";
          for(int i=0; i<target_slice.size(); i++){
            printf("%02X ",(u_char)target_slice[i]);
          }
          cout << endl;

          cout << "ikey: ";
          for(int i=0; i<ikey.user_key().size(); i++){
            printf("%02X ",(u_char)ikey.user_key()[i]);
          }
          cout << endl;

          //check row index number
          char index_num[indexnum_size];
          memcpy(index_num,ikey_data,indexnum_size);
          if(memcmp(snippet_->index_num, index_num, indexnum_size) != 0){//출력 지우지 말기
            cout << "different index number: ";
            for(int i=0; i<indexnum_size; i++){
              printf("(%02X %02X)",(u_char)snippet_->index_num[i],(u_char)index_num[i]);
            }
            cout << endl;
            index_valid = false;
            pk_valid = false;
          }
        }
        ipk++;

      }else{
        //go to next block
        pk_valid = false;
      }

    }
    
  }
  
}

int getPrimaryKeyData(const char* ikey_data, char* dest, list<PrimaryKey> pk_list){
  int offset = 4;
  int pk_length = 0;

  std::list<PrimaryKey>::iterator piter;
  for(piter = pk_list.begin(); piter != pk_list.end(); piter++){
      int key_length = (*piter).key_length;
      int key_type = (*piter).key_type;

      if(key_type == 3 || key_type == 8){//little(int32,int64)
        char pk[key_length];

        pk[0] = 0x00;//ikey[80 00 00 00 00 00 00 01]->ikey[00 00 00 00 00 00 00 01]
        for(int i = 0; i < key_length; i++){
          pk[i] = ikey_data[offset+key_length-i-1];
          //ikey[00 00 00 00 00 00 00 01]->dest[01 00 00 00 00 00 00 00]
        }

        memcpy(dest+pk_length, pk, key_length);

        pk_length += key_length;
        offset += key_length;

      }else if(key_type == 14){//little(newdate)
        char pk[key_length];

        for(int i = 0; i < key_length; i++){
          pk[i] = ikey_data[offset+key_length-i-1];
          //ikey[0F 98 8C]->dest[8C 98 0F]
        }

        memcpy(dest+pk_length, pk, key_length);

        pk_length += key_length;
        offset += key_length;

      }else if(key_type == 254 || key_type == 246){//big(string), decimal
        //ikey_data[39 33 37 2D 32 34 31 2D 33 31 39 38 20 20 20]
        //dest[39 33 37 2D 32 34 31 2D 33 31 39 38 20 20 20]

        memcpy(dest+pk_length, ikey_data+offset, key_length);

        pk_length += key_length;
        offset += key_length;

      }else if(key_type == 15){//big(varstring)
        char pk[key_length];
        int var_key_length = 0;
        bool end = false;

        /*ikey_data[33 34 35 33 31 32 38 35 {03} 33 34 36 {20 20 20 20 20} {02}]
          dest[{0B} 33 34 35 33 31 32 38 35 33 34 36]*/
        while(!end){
          if(ikey_data[offset] == sep || ikey_data[offset] == gap){//03 or 20
            offset++;
          }else if(ikey_data[offset] == fin){//02
            offset++;   
            end = true;  
          }else{
            pk[var_key_length] = ikey_data[offset];
            offset++;
            var_key_length++;
          }
        }

        if(var_key_length < 255){
          char len = (char)var_key_length;
          dest[pk_length] = len;
          pk_length += 1;
        }else{
          char len[2];
          int l1 = var_key_length / 256;
          int l2 = var_key_length % 256;
          len[0] = (char)l1;
          len[1] = (char)l2;
          memcpy(dest+pk_length,len,2);
          pk_length += 2;
        }

        memcpy(dest+pk_length, pk, var_key_length);
        pk_length += var_key_length;

      }else{
        cout << "[Scan] New Type!!!! - " << key_type << endl;
      }
  }

  return pk_length;
}

void Scan::EnQueueData(Result scan_result, Snippet snippet_){
    FilterInfo filterInfo(
      snippet_.table_filter, snippet_.table_col, snippet_.table_offset,
      snippet_.table_offlen, snippet_.table_datatype, snippet_.colindexmap, 
      snippet_.column_filter, snippet_.column_projection, 
      snippet_.groupby_col, snippet_.orderby_col
    );

    if(snippet_.scan_type == Full_Scan_Filter){
      if(scan_result.length != 0){//scan->filter
        Result scanResult(
          snippet_.work_id, /*scan_result.block_id_list,*/ scan_result.row_count, scan_result.length, 
          scan_result.data, scan_result.row_offset, snippet_.csd_name, filterInfo, 
          snippet_.total_block_count, scan_result.result_block_count, scan_result.last_valid_block_id
        );
        FilterQueue.push_work(scanResult);        
      }else{//scan->merge
        Result scanResult(
          snippet_.work_id, /*scan_result.block_id_list,*/ scan_result.row_count, scan_result.length, 
          scan_result.data, scan_result.row_offset, snippet_.csd_name, filterInfo,
          snippet_.total_block_count, scan_result.result_block_count, scan_result.last_valid_block_id
        );
        MergeQueue.push_work(scanResult);
      }

    }else if(snippet_.scan_type == Full_Scan){//scan->merge
      Column_Filtering(scan_result,snippet_);
      Result scanResult(
        snippet_.work_id, /*scan_result.block_id_list,*/ scan_result.row_count, scan_result.length, 
        scan_result.data, scan_result.row_offset, snippet_.csd_name, filterInfo,
        snippet_.total_block_count, scan_result.result_block_count, scan_result.last_valid_block_id
      );
      MergeQueue.push_work(scanResult);

    }else if(snippet_.scan_type == Index_Scan_Filter){//scan->filter
      if(scan_result.length != 0){
        Result scanResult(
          snippet_.work_id, /*scan_result.block_id_list,*/ scan_result.row_count, scan_result.length, 
          scan_result.data, scan_result.row_offset, snippet_.csd_name, filterInfo, 
          snippet_.total_block_count, scan_result.result_block_count, scan_result.last_valid_block_id
        );
        FilterQueue.push_work(scanResult);
      }else{
        Result scanResult(
          snippet_.work_id, /*scan_result.block_id_list,*/ scan_result.row_count, scan_result.length, 
          scan_result.data, scan_result.row_offset, snippet_.csd_name, filterInfo,
          snippet_.total_block_count, scan_result.result_block_count, scan_result.last_valid_block_id
        );
        MergeQueue.push_work(scanResult);
      }
      
    }else{//Index_Scan, scan->merge
      Column_Filtering(scan_result,snippet_);
      Result scanResult(
        snippet_.work_id, /*scan_result.block_id_list,*/ scan_result.row_count, scan_result.length, 
        scan_result.data, scan_result.row_offset, snippet_.csd_name, filterInfo,
        snippet_.total_block_count, scan_result.result_block_count, scan_result.last_valid_block_id
      );
      MergeQueue.push_work(scanResult);
    }
}


void Scan::Column_Filtering(Result scan_result, Snippet snippet_){
  if(snippet_.column_filter.size() == 0 || scan_result.length == 0){
    return;
  }

  int varcharflag = 1;
  scan_result.column_name = snippet_.column_filter;
  int max = 0;
  vector<int> newoffset;
  vector<vector<int>> row_col_offset;
  char tmpdatabuf[BUFF_SIZE];
  int newlen = 0;

  for(int i = 0; i < snippet_.column_filter.size(); i++){
    int tmp = snippet_.colindexmap[snippet_.column_filter[i]];
    if(tmp > max){
      max = tmp;
    }
  }

  for(int i = 0; i < scan_result.row_count; i++){
    int rowlen;
    if(i == scan_result.row_count - 1){
      rowlen = scan_result.length - scan_result.row_offset[i];
    }else{
      rowlen = scan_result.row_offset[i+1] - scan_result.row_offset[i];
    }

    // scan_result.row_offset[i];
    char tmpbuf[rowlen];
    memcpy(tmpbuf,scan_result.data + scan_result.row_offset[i], rowlen);
    GetColumnOff(tmpbuf, snippet_, max, varcharflag);
    newoffset.push_back(newlen);
    vector<int> tmpvector;

    for(int j = 0; j < snippet_.column_filter.size(); j++){
      memcpy(tmpdatabuf + newlen, tmpbuf + newoffmap[snippet_.table_col[i]], newlenmap[snippet_.table_col[i]]);
      tmpvector.push_back(newlen);
      newlen += newlenmap[snippet_.table_col[i]];
    }

    row_col_offset.push_back(tmpvector);
    free(tmpbuf);
  }

  memcpy(scan_result.data,tmpdatabuf,newlen);
  scan_result.length = newlen;
  scan_result.row_column_offset = row_col_offset;
  scan_result.row_offset = newoffset;
}


void Scan::GetColumnOff(char* data, Snippet snippet_, int colindex, int &varcharflag){
  if(varcharflag == 0){
    return;
  }
  newlenmap.clear();
  newoffmap.clear();
  varcharflag = 0;
  for(int i = 0; i < colindex; i++){
    if(snippet_.table_datatype[i] == 15){
      if(snippet_.table_offlen[i] < 256){
        if(i == 0){
          newoffmap.insert(make_pair(snippet_.table_col[i],snippet_.table_offset[i]));
        }else{
          newoffmap.insert(make_pair(snippet_.table_col[i],newoffmap[snippet_.table_col[i - 1]] + newlenmap[snippet_.table_col[i - 1]] + 2));
        }
        int8_t len = 0;
        int8_t *lenbuf;
        char membuf[1];
        memset(membuf, 0, 1);
        membuf[0] = data[newoffmap[snippet_.table_col[i]]];
        lenbuf = (int8_t *)(membuf);
        len = lenbuf[0];
        newlenmap.insert(make_pair(snippet_.table_col[i],len));
        free(membuf);
      }else{
        int16_t len = 0;
        int16_t *lenbuf;
        char membuf[2];
        memset(membuf, 0, 2);
        membuf[0] = data[newoffmap[snippet_.table_col[i]]];
        membuf[1] = data[newoffmap[snippet_.table_col[i]] + 1];
        lenbuf = (int16_t *)(membuf);
        len = lenbuf[0];
        newlenmap.insert(make_pair(snippet_.table_col[i],len));
        free(membuf);
      }
      varcharflag = 1;
    }else{
      if(varcharflag){
        newoffmap.insert(make_pair(snippet_.table_col[i],newoffmap[snippet_.table_col[i - 1]] + newlenmap[snippet_.table_col[i - 1]]));
        newlenmap.insert(make_pair(snippet_.table_col[i],snippet_.table_offlen[i]));
      }else{
        newoffmap.insert(make_pair(snippet_.table_col[i],snippet_.table_offset[i]));
        newlenmap.insert(make_pair(snippet_.table_col[i],snippet_.table_offlen[i]));
      }
    }
  }
}
