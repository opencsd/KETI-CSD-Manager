g++ -fno-rtti main.cc -omain ../host_file_scan/rocksdb/librocksdb.a -I../host_file_scan/rocksdb/include -L/usr/local/lib -lzstd -lsnappy -lz -llz4 -lbz2 -ldl -O2 -std=c++17 -lpthread