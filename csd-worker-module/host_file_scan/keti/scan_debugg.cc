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
bool check = true;
char origin_index_num[indexnum_size];

char sep = 0x03;
char gap = 0x20;
char fin = 0x02;

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
  cout << "<-----------  Scan Layer Running...  ----------->\n";
    while (1){
        Snippet snippet = ScanQueue.wait_and_pop();
        
        TableRep table_rep = CSDTableManager_.GetTableRep(snippet.table_name);
        kNumInternalBytes_ = snippet.kNumInternalBytes;
		                
        Options options;
        SstFileReader sstFileReader(options);

        FilterInfo filterInfo(
          snippet.table_col, snippet.table_offset, snippet.table_offlen,
          snippet.table_datatype, snippet.colindexmap, /*snippet.filtered_col,
          snippet.filtered_datatype,*/ snippet.table_filter,
          snippet.column_projection, snippet.projection_datatype, snippet.groupby_col
        );
        Result scanResult(snippet.query_id, snippet.work_id, snippet.csd_name, 
          snippet.total_block_count, filterInfo      
        );

        BlockScan(&sstFileReader, &snippet, &scanResult);
        cout << "[SCAN] Block Scan -> EnqueueData" << endl;
        EnQueueData(scanResult, snippet);
        cout << "[SCAN] EnQueueData -> scanResult.InitResult" << endl;
        scanResult.InitResult();
    }
}

void Scan::BlockScan(SstFileReader* sstBlockReader_, Snippet *snippet_, Result *scan_result){
  Status s  = sstBlockReader_->Open(snippet_->file_path);
  if(!s.ok()){
      cout << "open error" << endl;
  }

  const char* ikey_data;
  const char* row_data;
  size_t row_size;

  Iterator* datablock_iter = sstBlockReader_->NewIterator(ReadOptions());

  int row_cnt = 0;

  if(snippet_->scan_type == Full_Scan_Filter || snippet_->scan_type == Full_Scan){//full table scan

    for (datablock_iter->SeekToFirst(); datablock_iter->Valid(); datablock_iter->Next()) {//iterator first부터 순회
      row_cnt++;
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

      ikey_data = ikey.user_key().data();//tableIndexNum
      row_data = value.data();
      row_size = value.size();
      
      if(check){//index num 획득 임시 추가
        memcpy(origin_index_num,ikey_data,indexnum_size);
        check = false;
      }

      //check row index number
      char index_num[indexnum_size];
      memcpy(index_num,ikey_data,indexnum_size);
      if(memcmp(origin_index_num/*snippet_->index_num*/, index_num, indexnum_size) != 0){//출력 지우지 말기
        cout << "different index number: ";
        for(int i=0; i<indexnum_size; i++){
          printf("(%02X %02X)",(u_char)origin_index_num[i],(u_char)index_num[i]);
        }
        cout << endl;
        index_valid = false;
        return;
      }

      first_row = false;//블록의 첫번째 row부터 invalid인지 여부

      std::cout << "[Row" << row_cnt << "(HEX)] KEY: " << ikey.user_key().ToString(true) << " | VALUE: " << value.ToString(true) << endl;

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

      // //(수정) 테스트 출력
      // cout << "\n---Row("<< row_cnt << ")--\n";
      // for(int i = 0; i < scan_result->length; i++){
      //     printf("%02X",(u_char)scan_result->data[i]);
      // }
      // cout << "\n---\n";

      // scanrownum = scan_result->row_count;
      // if(scanrownum < bfscanrow){
      //   totalrownum += bfscanrow;
      // }
      // bfscanrow = scanrownum;
      // cout << totalrownum << endl;

      if(row_cnt > 2){
        return;
      }
    } 

  }else{//index scan
    // string pk_str = snippet_->index_pk;
    // Document document;
    // document.Parse(pk_str.c_str());
    // Value &index_pk = document["index_pk"];

    // vector<char> target_pk;
    // target_pk.assign(snippet_->index_num,snippet_->index_num+4);

    // bool pk_valid = true;

    // while(pk_valid){
    //   for(int i=0; i<index_pk.Size(); i++){
    //     int key_type = snippet_->table_datatype[i];

    //     if(key_type == MySQL_INT32 || key_type == MySQL_DATE){//int(int, date)
    //       int key_length = snippet_->table_offlen[i];
    //       union{
    //         int value;
    //         char byte[4];
    //       }pk;
    //       pk.value = index_pk[i][ipk].GetInt();
    //       for(int j=0; j<key_length; j++){
    //         target_pk.push_back(pk.byte[j]);
    //       }

    //     }else if(key_type == MySQL_INT64 || key_type == 246){//int64_t(bigint, decimal)
    //       int key_length = snippet_->table_offlen[i];
    //       union{
    //         int64_t value;
    //         char byte[8];
    //       }pk;
    //       pk.value = index_pk[i][ipk].GetInt64();
    //       for(int j=0; j<key_length; j++){
    //         target_pk.push_back(pk.byte[j]);
    //       }

    //     }else if(key_type == 254 || key_type == 15){//string(string, varstring)
    //       string pk = index_pk[i][ipk].GetString();
    //       int key_length = pk.length();
    //       for(int j=0; j<key_length; j++){
    //         target_pk.push_back(pk[j]);
    //       }
    //     }
    //   }

    //   char *p = &*target_pk.begin();
    //   Slice target_slice(p,target_pk.size());

    //   datablock_iter->Seek(target_slice);

    //   if(datablock_iter->Valid()){//target과 pk가 같은지 한번더 비교
    //     const Slice& key = datablock_iter->key();
    //     const Slice& value = datablock_iter->value();

    //     InternalKey ikey;
    //     ikey.DecodeFrom(key);
    //     ikey_data = ikey.user_key().data();

    //     //테스트 출력
    //     std::cout << "[Row(HEX)] KEY: " << ikey.user_key().ToString(true) << " | VALUE: " << value.ToString(true) << endl;

    //     if(target_slice.compare(ikey.user_key()) == 0){ //target O
    //       row_data = value.data();
    //       row_size = value.size();

    //       char total_row_data[snippet_->primary_length+row_size];
    //       int pk_length;

    //       pk_length = getPrimaryKeyData(ikey_data, total_row_data, snippet_->primary_key_list);//key
        
    //       memcpy(total_row_data + pk_length, row_data, row_size);//key+value
    //       memcpy(scan_result->data + scan_result->length, total_row_data, pk_length + row_size);//buff+key+value
          
    //       scan_result->length += row_size + pk_length;
    //       scan_result->row_count++;
          
    //     }else{//target X
    //       //primary key error(출력 지우지 말기)
    //       cout << "primary key error [no primary key!]" << endl;      

    //       cout << "target: ";
    //       for(int i=0; i<target_slice.size(); i++){
    //         printf("%02X ",(u_char)target_slice[i]);
    //       }
    //       cout << endl;

    //       cout << "ikey: ";
    //       for(int i=0; i<ikey.user_key().size(); i++){
    //         printf("%02X ",(u_char)ikey.user_key()[i]);
    //       }
    //       cout << endl;

    //       //check row index number
    //       char index_num[indexnum_size];
    //       memcpy(index_num,ikey_data,indexnum_size);
    //       if(memcmp(snippet_->index_num, index_num, indexnum_size) != 0){//출력 지우지 말기
    //         cout << "different index number: ";
    //         for(int i=0; i<indexnum_size; i++){
    //           printf("(%02X %02X)",(u_char)snippet_->index_num[i],(u_char)index_num[i]);
    //         }
    //         cout << endl;
    //         index_valid = false;
    //         pk_valid = false;
    //       }
    //     }
    //     ipk++;

    //   }else{
    //     //go to next block
    //     pk_valid = false;
    //   }

    // }
    
  }


  
}

int getPrimaryKeyData(const char* ikey_data, char* dest, list<PrimaryKey> pk_list){
  int offset = 4;
  int pk_length = 0;

  std::list<PrimaryKey>::iterator piter;
  for(piter = pk_list.begin(); piter != pk_list.end(); piter++){
      int key_length = (*piter).key_length;
      int key_type = (*piter).key_type;

      switch(key_type){
        case MySQL_INT32:
        case MySQL_INT64:{
          char pk[key_length];
          pk[0] = 0x00;//ikey[80 00 00 00 00 00 00 01]->ikey[00 00 00 00 00 00 00 01]
          for(int i = 0; i < key_length; i++){
            pk[i] = ikey_data[offset+key_length-i-1];
            //ikey[00 00 00 00 00 00 00 01]->dest[01 00 00 00 00 00 00 00]
          }
          memcpy(dest+pk_length, pk, key_length);
          pk_length += key_length;
          offset += key_length;
          break;
        }case MySQL_DATE:{
          char pk[key_length];
          for(int i = 0; i < key_length; i++){
            pk[i] = ikey_data[offset+key_length-i-1];
            //ikey[0F 98 8C]->dest[8C 98 0F]
          }
          memcpy(dest+pk_length, pk, key_length);
          pk_length += key_length;
          offset += key_length;
          break;
        }case MySQL_NEWDECIMAL:
         case MySQL_STRING:{
          //ikey_data[39 33 37 2D 32 34 31 2D 33 31 39 38 20 20 20]
          //dest[39 33 37 2D 32 34 31 2D 33 31 39 38 20 20 20]
          memcpy(dest+pk_length, ikey_data+offset, key_length);
          pk_length += key_length;
          offset += key_length;
          break;
        }case MySQL_VARSTRING:{
          char pk[key_length];
          int var_key_length = 0;
          bool end = false;
          /*ikey_data[33 34 35 33 31 32 38 35 {03} 33 34 36 (20 20 20 20 20) {02}]
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
          if(var_key_length < 256){
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
          break;
        }default:{
          cout << "new type!!-" << key_type << endl;
        }
      }
  }

  return pk_length;
}

void Scan::EnQueueData(Result scan_result, Snippet snippet_){
    // Result scanResult = scan_result;

    if(snippet_.scan_type == Full_Scan_Filter){
      if(scan_result.length != 0){//scan->filter
        cout << "[SCAN] EnQueueData > FilterQueue.push_work" << endl;
        FilterQueue.push_work(scan_result);        
      }else{//scan->merge
        cout << "[SCAN] EnQueueData > MergeQueue.push_work" << endl;
        MergeQueue.push_work(scan_result);
      }

    }else if(snippet_.scan_type == Full_Scan){//scan->merge
      //(수정)scan의 column filtering 삭제
      // if(scan_result.filter_info.need_col_filtering){
      //   cout << "[SCAN] EnQueueData > Column_Filtering" << endl;
      //   Column_Filtering(&scan_result, snippet_);
      // }
      cout << "[SCAN] EnQueueData > MergeQueue.push_work" << endl;
      MergeQueue.push_work(scan_result);

    }else if(snippet_.scan_type == Index_Scan_Filter){//scan->filter
      if(scan_result.length != 0){
        FilterQueue.push_work(scan_result);
      }else{
        MergeQueue.push_work(scan_result);
      }
      
    }else{//Index_Scan, scan->merge
      // if(scan_result.filter_info.need_col_filtering){
      //   Column_Filtering(&scan_result, snippet_);
      // }
      MergeQueue.push_work(scan_result);
    }
}


// void Scan::Column_Filtering(Result *scan_result, Snippet snippet_){
//   cout << "[SCAN] Column_Filtering" << endl;
//   if(scan_result->length == 0){
//     return;
//   }

//   int varcharflag = 1;
//   // scan_result->column_name = snippet_->column_filter;
//   int max = 0;
//   vector<int> newoffset;
//   vector<vector<int>> row_col_offset;
//   char tmpdatabuf[BUFF_SIZE];
//   int newlen = 0;

//   for(int i = 0; i < scan_result->filter_info.filtered_col.size(); i++){
//     int tmp = scan_result->filter_info.colindexmap[scan_result->filter_info.filtered_col[i]];
//     if(tmp > max){
//       max = tmp;
//     }
//   }

//   for(int i = 0; i < scan_result->row_count; i++){
//     int rowlen;
//     if(i == scan_result->row_count - 1){
//       rowlen = scan_result->length - scan_result->row_offset[i];
//     }else{
//       rowlen = scan_result->row_offset[i+1] - scan_result->row_offset[i];
//     }

//     // scan_result.row_offset[i];
//     char tmpbuf[rowlen];
//     memcpy(tmpbuf,scan_result->data + scan_result->row_offset[i], rowlen);
//     GetColumnOff(tmpbuf, snippet_, max, varcharflag);
//     newoffset.push_back(newlen);
//     vector<int> tmpvector;

//     for(int j = 0; j < scan_result->filter_info.filtered_col.size(); j++){
//       memcpy(tmpdatabuf + newlen, tmpbuf + newoffmap[scan_result->filter_info.table_col[i]], newlenmap[snippet_.table_col[i]]);
//       tmpvector.push_back(newlen);
//       newlen += newlenmap[snippet_.table_col[i]];
//     }

//     row_col_offset.push_back(tmpvector);
//     // free(tmpbuf);
//   }

//   memset(&scan_result->data, 0, sizeof(BUFF_SIZE));
//   memcpy(scan_result->data,tmpdatabuf,newlen);
//   scan_result->length = newlen;
//   scan_result->row_column_offset = row_col_offset;
//   scan_result->row_offset = newoffset;
// }


// void Scan::GetColumnOff(char* data, Snippet snippet_, int colindex, int &varcharflag){
//   cout << "[SCAN] GetColumnOff" << endl;
//   if(varcharflag == 0){
//     return;
//   }
//   newlenmap.clear();
//   newoffmap.clear();
//   varcharflag = 0;
//   for(int i = 0; i < colindex; i++){
//     if(snippet_.table_datatype[i] == 15){
//       if(snippet_.table_offlen[i] < 256){
//         if(i == 0){
//           newoffmap.insert(make_pair(snippet_.table_col[i],snippet_.table_offset[i]));
//         }else{
//           newoffmap.insert(make_pair(snippet_.table_col[i],newoffmap[snippet_.table_col[i - 1]] + newlenmap[snippet_.table_col[i - 1]] + 2));
//         }
//         int8_t len = 0;
//         int8_t *lenbuf;
//         char membuf[1];
//         memset(membuf, 0, 1);
//         membuf[0] = data[newoffmap[snippet_.table_col[i]]];
//         lenbuf = (int8_t *)(membuf);
//         len = lenbuf[0];
//         newlenmap.insert(make_pair(snippet_.table_col[i],len));
//         // free(membuf);
//       }else{
//         int16_t len = 0;
//         int16_t *lenbuf;
//         char membuf[2];
//         memset(membuf, 0, 2);
//         membuf[0] = data[newoffmap[snippet_.table_col[i]]];
//         membuf[1] = data[newoffmap[snippet_.table_col[i]] + 1];
//         lenbuf = (int16_t *)(membuf);
//         len = lenbuf[0];
//         newlenmap.insert(make_pair(snippet_.table_col[i],len));
//         // free(membuf);
//       }
//       varcharflag = 1;
//     }else{
//       if(varcharflag){
//         newoffmap.insert(make_pair(snippet_.table_col[i],newoffmap[snippet_.table_col[i - 1]] + newlenmap[snippet_.table_col[i - 1]]));
//         newlenmap.insert(make_pair(snippet_.table_col[i],snippet_.table_offlen[i]));
//       }else{
//         newoffmap.insert(make_pair(snippet_.table_col[i],snippet_.table_offset[i]));
//         newlenmap.insert(make_pair(snippet_.table_col[i],snippet_.table_offlen[i]));
//       }
//     }
//   }
// }

// void Scan::IndexScan(SstBlockReader* sstBlockReader_, BlockInfo* blockInfo, Snippet *snippet_, Result *scan_result){
// }
