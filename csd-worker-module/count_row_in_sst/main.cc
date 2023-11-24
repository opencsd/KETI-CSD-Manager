// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory)
#include <iostream>
#include <string>
#include <stdio.h>

#include "rocksdb/sst_file_reader.h"
#include "rocksdb/slice.h"
#include "rocksdb/iterator.h"
#include "rocksdb/cache.h"
#include "rocksdb/options.h"
#include "rocksdb/status.h"
#include "rocksdb/slice.h"

using namespace rocksdb;
using namespace std;

bool has_pk = false; //pk 유무에 따라 설정하기
uint64_t kNumInternalBytes_;

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

int main(int argc, char **argv){
  int total_row = 0;
  bool index_valid;
  bool check = true;
  int indexnum_size = 4;
  char origin_index_num[indexnum_size];

  std::string filename = argv[1];
  const char *file_path = filename.c_str();

  if(has_pk){
    kNumInternalBytes_ = 0;
  }else{
    kNumInternalBytes_ = 8;
  }

  Options options;
  SstFileReader sstFileReader(options);

  Status s  = sstFileReader.Open(file_path);
  if(!s.ok()){
      cout << "open error" << endl;
  }

  const char* ikey_data;
  const char* row_data;
  size_t row_size;

  Iterator* datablock_iter = sstFileReader.NewIterator(ReadOptions());

  for (datablock_iter->SeekToFirst(); datablock_iter->Valid(); datablock_iter->Next()) {//iterator first부터 순회
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
      break;
    }

    total_row++;

    //std::cout << "[Row(HEX)] KEY: " << key.ToString(true) << " | VALUE: " << value.ToString(true) << endl;
  }
  
  printf("= Total Row Count : %d\n", total_row);
  if(index_valid) cout << "Index Invalid" << endl;

  return 0;
}