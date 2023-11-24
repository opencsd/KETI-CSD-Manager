#include <iostream>
#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <vector>

using namespace std;

#include "rocksdb/sst_file_reader.h"

struct TableRep{
    string dev_name;
    bool blocks_maybe_compressed;
    bool blocks_definitely_zstd_compressed;
    bool immortal_table;
    uint32_t read_amp_bytes_per_bit;
    string compression_name;
};

class TableManager{
public:
    TableManager(){}
    
    void InitCSDTableManager();//임시로 데이터 넣어놓음
    TableRep GetTableRep(string table_name);

private:
    unordered_map<string,struct TableRep> table_rep_;// key=table_name
};