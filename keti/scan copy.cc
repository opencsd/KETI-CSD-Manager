// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory)
#include "scan.h"

uint64_t kNumInternalBytes_;
const int indexnum_size = 4;
bool index_valid;
bool first_row;
int scanrownum = 0;
int bfscanrow = 0;
int totalrownum = 0;

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
        int ipk = 0;

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
                  scanResult.block_id_list.second.push_back(blockInfo.block_id);
                  scanResult.last_valid_block_id = blockInfo.block_id;
                  cout << "<invalid> not first row / " << scanResult.last_valid_block_id << "/" << scanResult.result_block_count << endl;
              }
              scanResult.result_block_count += snippet.total_block_count - current_block_count;
              current_block_count = snippet.total_block_count;
            }else{
              scanResult.block_id_list.second.push_back(blockInfo.block_id);
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
  // cout << "[Block Scan Start]" << endl;
  // cout << "- WorkID: " << snippet_.work_id << endl;
  // cout << "- Block( " << block_count_ << " / " << snippet_.block_info_list.size() << " ) ID: " << blockInfo->block_id << endl;  
  // cout << "- Block Size: " << blockInfo->block_size << endl;
  
  // printf("(id:%d|offset:%ld|size:%ld)\n",blockInfo->block_id, blockInfo->block_offset, blockInfo->block_size);

  // int dev_fd = open("/dev/ngd-blk2", O_RDONLY);
  // lseek(dev_fd,blockInfo->block_offset,SEEK_SET);
  // char temp_buf[4096];
  // memset(temp_buf,0,sizeof(temp_buf));
  // uint64_t read_size = read(dev_fd, temp_buf, blockInfo->block_size);
  // cout << "read_size : " << read_size << endl;
  // if(read_size !=  blockInfo->block_size){
  //   std::cout << "read error" << std::endl;
  // }
  // close(dev_fd);
  
  // cout << "\n----------------------------------------------\n";
  // for(int i = 0; i < blockInfo->block_size; i++){
  //     printf("%02X",(u_char)temp_buf[i]);
  // }
  // cout << "\n------------------------------------------------\n";


  Status s  = sstBlockReader_->Open(blockInfo);
  if(!s.ok()){
      cout << "open error" << endl;
  }

  const char* ikey_data;
  const char* row_data;
  size_t row_size;

  Iterator* datablock_iter = sstBlockReader_->NewIterator(ReadOptions());

  for (datablock_iter->SeekToFirst(); datablock_iter->Valid(); datablock_iter->Next()) {
      // cout << scan_result->row_offset.size() << endl;
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

      row_data = value.data();
      row_size = value.size();
      ikey_data = ikey.user_key().data();
              
      std::cout << "key: " << key.ToString(true) << " | ikey: " << ikey.user_key().ToString(true) << endl;

      char index_num[indexnum_size];
      if(check){
        // cout << "origin_index_num: ";
        memcpy(origin_index_num,ikey_data,indexnum_size);
        // for(int i=0; i<indexnum_size; i++){
        //   printf("%02X ",(u_char)origin_index_num[i]);
        // }
        // cout << endl;
        check = false;
        // std::cout << "[Row(HEX)] KEY: " << ikey.user_key().ToString(true) << " | VALUE: " << value.ToString(true) << endl;
      }

      memcpy(index_num,ikey_data,indexnum_size);
      
      if(memcmp(origin_index_num, index_num, indexnum_size) != 0){
        cout << "different ";
        // for(int i=0; i<indexnum_size; i++){
        //   printf("(%02X %02X)",(u_char)origin_index_num[i],(u_char)index_num[i]);
        // }
        cout << endl;
        index_valid = false;
        return;
      }

      first_row = false;
      // std::cout << "[Row(HEX)] KEY: " << ikey.user_key().ToString(true) << " | VALUE: " << value.ToString(true) << endl;

      if(snippet_->primary_length != 0){
        char total_row_data[snippet_->primary_length+row_size];
        int pk_length;

        pk_length = getPrimaryKeyData(ikey_data, total_row_data, snippet_->primary_key_list);
       
        memcpy(total_row_data + pk_length, row_data, row_size);
        memcpy(scan_result->data + scan_result->length, total_row_data, pk_length + row_size);
        
        scan_result->length += row_size + pk_length;
        scan_result->row_count++;

        // char total_row_data[row_size+snippet_->total_primary_size];
        // int primary_key_offset = 0;

        // std::list<PrimaryKey>::iterator piter;
        // for(piter = snippet_->primary_key_list.begin(); piter != snippet_->primary_key_list.end(); piter++){
        //     // std::cout << "primary key name: " << (*piter).key_name;
        //     // std::cout << " | primary key size: " << (*piter).key_length;
        //     // std::cout << " | primary key type: " << (*piter).key_type << std::endl;

        //     int key_length = (*piter).key_length;
        //     int key_type = (*piter).key_type;

        //     if(key_type == 8 || key_type == 4){//big int or int
        //         char primary_key[key_length];

        //         for(int i = 0; i < key_length; i++){
        //           // printf("%02X ",(u_char)ikey_data[i+indexnum_size+primary_key_offset]);
        //           primary_key[i] = ikey_data[i+indexnum_size+primary_key_offset];
        //         }//primary_key[80 00 00 00 00 00 00 01 ]

        //         std::bitset<8> bs(primary_key[0]);//8 하드코딩!!!!!!!!!!!!!!!!!!!!!!!
        //         // std::cout << "bs:" << bs << std::endl; // int_binary:10000000
        //         bs ^= 1 << (key_length-1);
        //         // std::cout << "bs:" << bs << std::endl; // int_binary:00000000
        //         primary_key[0] =  (int)(bs.to_ulong());

        //         // printf("primary_key[0]: %02X ",primary_key[0]); //primary_key[0]: 00
        //         // std::cout << " primary_key["; // primary_key[00 00 00 00 00 00 00 01 ]
        //         // for(int i = 0; i < key_length; i++){
        //         //     printf("%02X ",(u_char)primary_key[i]);
        //         // }
        //         // std::cout << "]\n";

        //         // long my_value = *((long *)primary_key);
        //         // std::cout << "my_value " << my_value << std::endl;;

        //         char reversed_primary_key[key_length];
        //         for(int i = 0; i < key_length; i++){
        //             reversed_primary_key[i] = primary_key[key_length-i-1];
        //             // printf("(%02X %02X)",(u_char)reversed_primary_key[i], (u_char)primary_key[key_length-i-1]);
        //         }
        //         // std::cout << std::endl;

        //         // my_value = *((long *)reversed_primary_key);
        //         // std::cout << "my_value " << my_value << std::endl;;

        //         memcpy(total_key_value+primary_key_offset, reversed_primary_key, key_length);
        //         // std::cout << " total data[";
        //         // for(int i = 0; i < key_length+primary_key_offset; i++){
        //         //     printf("%02X",(u_char)total_key_value[i]);
        //         // }
        //         // std::cout << "]\n";

        //         primary_key_offset += key_length;
        //     }else if(key_type == 14){
        //       // 다른 타입일 경우
        //     } 

        //   } 
        //   memcpy(total_key_value+snippet_->total_primary_size, row_data, row_size);

        //   // std::cout << "total_key_value[";
        //   // for(int i = 0; i < row_size+snippet_.total_primary_size; i++){
        //   //     printf("%02X",(u_char)total_key_value[i]);
        //   // }
        //   // std::cout << "]\n";

        //   memcpy(scan_result->data+scan_result->length, total_key_value, row_size+snippet_->total_primary_size);
        //   scan_result->length += row_size+snippet_->total_primary_size;
        //   scan_result->row_count++;
        //   // std::cout << "new data[";
        //   // for(int i = 0; i < scan_result.length; i++){
        //   //     printf("%02X",(u_char)scan_result.data[i]);
        //   // }
        //   // std::cout << "]\n";
      }else{
          memcpy(scan_result->data+scan_result->length, row_data, row_size);
          scan_result->length += row_size;
          scan_result->row_count++;
      }

      scanrownum = scan_result->row_count;
      if(scanrownum < bfscanrow){
        totalrownum += bfscanrow;
      }
      bfscanrow = scanrownum;
      cout << totalrownum << endl;

      // cout << "--" << endl;
  } 
  

  // cout << "---------------Scanned Block Info-----------------" << endl;
  // cout << "| work id: " << scan_result.work_id << " | length: " << scan_result.length
  //      << " | rows: " << scan_result.row_count << endl;
  // cout << "[ " << scan_result.block_id_list.first << " : [";
  // list<int>::iterator iter2;
  // for(iter2 = scan_result.block_id_list.second.begin(); iter2 != scan_result.block_id_list.second.end(); iter2++){
  //     cout << (*iter2) << " ";
  // }
  // cout << "] ]" << endl;
  
  // cout << "scan_result.length: " << scan_result.length << endl;
  // cout << "\n----------------------------------------------\n";
  // for(int i = 0; i < scan_result.length; i++){
  //     printf("%02X",(u_char)scan_result.data[i]);
  // }
  // cout << "\n------------------------------------------------\n";

}

void Scan::EnQueueData(Result scan_result, Snippet snippet_){
    // cout << "Enqueue Data" << endl;
    // cout << "---------------Scanned Block Info-----------------" << endl;
    // cout << "| work id: " << scan_result.work_id << " | length: " << scan_result.length
    //      << " | rows: " << scan_result.row_count << endl;
    // cout << "[ " << scan_result.block_id_list.first << " : [";
    // list<int>::iterator iter2;
    // for(iter2 = scan_result.block_id_list.second.begin(); iter2 != scan_result.block_id_list.second.end(); iter2++){
    //     cout << (*iter2) << " ";
    // }
    // cout << "] ]" << endl;
    // cout << "scan_result.length: " << scan_result.length << endl;
    // cout << "\n----------------------------------------------\n";
    // for(int i = 0; i < scan_result.length; i++){
    //     printf("%02X",(u_char)scan_result.data[i]);
    // }
    // cout << "\n------------------------------------------------\n";
    if((snippet_.only_scan) || (scan_result.length == 0 && !index_valid)){//scan->buff
      MergeResult scanResult(
        snippet_.work_id, snippet_.csd_name, scan_result.block_id_list, scan_result.length, 
        scan_result.row_offset, scan_result.row_count, scan_result.data,
        snippet_.total_block_count, scan_result.result_block_count, scan_result.last_valid_block_id
      );
      ReturnQueue.push_work(scanResult);
    }else{//scan->filter
      FilterInfo filterInfo(
        snippet_.table_filter, snippet_.table_col, snippet_.table_offset,
        snippet_.table_offlen, snippet_.table_datatype, snippet_.colindexmap
      );
      Result scanResult(
        snippet_.work_id, scan_result.block_id_list, scan_result.row_count, scan_result.length, 
        scan_result.data, scan_result.row_offset, snippet_.csd_name, filterInfo, 
        snippet_.total_block_count, scan_result.result_block_count, scan_result.last_valid_block_id
      );
      FilterQueue.push_work(scanResult);
    }
}

int getPrimaryKeyData(const char* ikey_data, char* dest, list<PrimaryKey> pk_list){
  int offset = 4;
  int pk_length = 0;
  char sep = 0x03;
  char gap = 0x20;
  char fin = 0x02;

  std::list<PrimaryKey>::iterator piter;
  for(piter = pk_list.begin(); piter != pk_list.end(); piter++){
      int key_length = (*piter).key_length;
      int key_type = (*piter).key_type;

      if(key_type == 3 || key_type == 8){//little(int32,int64)
        char pk[key_length];

        for(int i = 0; i < key_length-1; i++){
          pk[i] = ikey_data[offset+key_length-i-1];
          //ikey[80 00 00 00 00 00 00 01]->dest[01 00 00 00 00 00 00]
        }
        pk[key_length-1] = 0x00;//dest[01 00 00 00 00 00 00 00]

        // std::bitset<8> bs(pk[0]);//8 하드코딩!!!!!!!!!!!!!!!!!!!!!!!
        // bs ^= 1 << (key_length-1);
        // pk[0] =  (int)(bs.to_ulong());

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

        // char pk[key_length];
        // for(int i=0; i < key_length; i++){
        //   pk[pk_length+i] = ikey_data[offset+i];
        // }

        memcpy(dest+pk_length, ikey_data+offset, key_length);

        pk_length += key_length;
        offset += key_length;

      }else if(key_type == 15){//big(varstring)
        char pk[key_length];
        int var_key_length = 0;
        bool end = false;

        /*ikey_data[33 34 35 33 31 32 38 35 {03} 33 34 36 '20 20 20 20 20' {02}]
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