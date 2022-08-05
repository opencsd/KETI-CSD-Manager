// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory)
#include "scan.h"

// constexpr uint64_t kNumInternalBytes = 8;

// inline Slice ExtractUserKey(const Slice& internal_key) {
//   assert(internal_key.size() >= kNumInternalBytes);
//   return Slice(internal_key.data(), internal_key.size() - kNumInternalBytes);
// }

// class InternalKey {
//  private:
//   string rep_;

//  public:
//   InternalKey() {}  // Leave rep_ as empty to indicate it is invalid
//   void DecodeFrom(const Slice& s) { rep_.assign(s.data(), s.size()); }
//   Slice user_key() const { return ExtractUserKey(rep_); }
// };

void Scan::Scanning(){
  //cout << "<-----------  Scan Layer Running...  ----------->\n";

    while (1){
        Snippet snippet = ScanQueue.wait_and_pop();

        //cout << "\n[Get Parsed Snippet] \n #Snippet Work ID: " << snippet.work_id << "" << endl;	  

        TableRep table_rep = CSDTableManager_.GetTableRep(snippet.table_name);
		                
        Options options;
        SstBlockReader sstBlockReader(
            options, table_rep.blocks_maybe_compressed, table_rep.blocks_definitely_zstd_compressed, 
            table_rep.immortal_table, table_rep.read_amp_bytes_per_bit, table_rep.dev_name);

        list<BlockInfo> &bl = snippet.block_info_list;
        list<BlockInfo>::iterator it;
        int block_count = 1;
        for(it = bl.begin(); it != bl.end(); it++){//블록
            BlockInfo b = *it;
            BlockScan(sstBlockReader, &b, snippet, block_count);
            block_count += 1;
        }
    }
}

void Scan::BlockScan(SstBlockReader& sstBlockReader_, BlockInfo* blockInfo, Snippet &snippet_, int bc){
  //cout << "\n[1.Block Scan Start]" << endl;
  // cout << "- WorkID: " << snippet_.work_id << endl;
  // cout << "- Block( " << bc << " / " << snippet_.block_info_list.size() << " ) ID: " << blockInfo->block_id << endl;  
  // cout << "- Block Size: " << blockInfo->block_size << endl;

  Status s  = sstBlockReader_.Open(blockInfo);
  if(!s.ok()){
      cout << "open error" << endl;
  }

  char data[4096];
  int row_count = 0;
  size_t length = 0;
  const char* row_data;
  size_t row_size;
  vector<int> row_offset;

  Iterator* datablock_iter = sstBlockReader_.NewIterator(ReadOptions());
    
  //cout << "\n[2.Read Block Rows...]\n" << endl;
  for (datablock_iter->SeekToFirst(); datablock_iter->Valid(); datablock_iter->Next()) {
      row_offset.push_back(length);

      Status s = datablock_iter->status();
      if (!s.ok()) {
      cout << "Error reading the block - Skipped \n";
      break;
      }
      
      const Slice& key = datablock_iter->key();
      const Slice& value = datablock_iter->value();

      // InternalKey ikey;
      // ikey.DecodeFrom(key);
      //std::cout << "[Row(HEX)] KEY: " << ikey.user_key().ToString(true) << " | VALUE: ";

      row_data = value.data();
      row_size = value.size();

      // for(int i=0; i<row_size; i++){
      //   printf("%02X",(u_char)row_data[i]);
      // }
      // cout << endl;

      memcpy(data+length,row_data,row_size);
      
      length += row_size;
      row_count += 1;
  }

  // cout << "-------------------------------Scan Result-----------------------------------------"<<endl;
  // cout << "| WorkID: " << snippet_.work_id <<
  //         " | BlockID( " << bc << " / " << snippet_.block_info_list.size() << " ) : " << blockInfo->block_id <<  
  //         " | scan size: " << length << " | row_count: " << row_count
  // << " | block size: " << blockInfo->block_size << " | " << endl; 
  // printf("#offset: [ ");
  // for(int j=0; j<row_count+1; j++){
  //     printf("%d ,", temp_offset[j]);
  // }
  // cout<< "]" << endl;
  // cout << "----------------------------------------------------------------------------------"<<endl;
  // for(int i=0; i<length; i++){
  //     printf("%02X",(u_char)data[i]);
  // }
  // cout << "\n----------------------------------------------------------------------------------"<<endl;

  EnQueueData(data, row_count, blockInfo->block_id, snippet_, row_offset, length); 

}

void Scan::EnQueueData(char *data_, int row_count_, int block_id_, 
                    Snippet snippet_, vector<int> row_offset_, int length_){
    if(snippet_.only_scan){//scan->buff
      MergeResult scanResult(
        snippet_.work_id, snippet_.csd_name, snippet_.total_block_count,
        block_id_, length_, row_offset_, row_count_, data_
      );
      ReturnQueue.push_work(scanResult);
    }
    else{//scan->filter
      FilterInfo filterInfo(
        snippet_.table_filter, snippet_.table_col, snippet_.table_offset,
        snippet_.table_offlen, snippet_.table_datatype, snippet_.colindexmap
      );
      Result scanResult(
        snippet_.work_id, block_id_, snippet_.total_block_count, 
        row_count_, length_, data_, row_offset_, 
        snippet_.csd_name, filterInfo
      );
      FilterQueue.push_work(scanResult);
    }
}