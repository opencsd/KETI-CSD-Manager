#include ../rocksdb/make_config.mk

ifndef DISABLE_JEMALLOC
	ifdef JEMALLOC
		PLATFORM_CXXFLAGS += -DROCKSDB_JEMALLOC -DJEMALLOC_NO_DEMANGLE
	endif
	EXEC_LDFLAGS := $(JEMALLOC_LIB) $(EXEC_LDFLAGS) -lpthread
	PLATFORM_CXXFLAGS += $(JEMALLOC_INCLUDE)
endif

ifneq ($(USE_RTTI), 1)
	CXXFLAGS += -fno-rtti
endif

CFLAGS += -Wstrict-prototypes

.PHONY: clean librocksdb

all: main

server: librocksdb server.cc
	$(CXX) $(CXXFLAGS) $@.cc -o$@ ../rocksdb/librocksdb.a -I../rocksdb/include -ldl -O2 -std=c++17 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)

clean:
	rm -rf ./server
	rm -rf ./test
	rm -rf ./sstopen

librocksdb:
	cd ../rocksdb && $(MAKE) static_lib

scan: librocksdb scan.cc
	$(CXX) $(CXXFLAGS) $@.cc  filter.cc  input.cc -o$@ ../rocksdb/librocksdb.a -I../rocksdb/include -ldl -O2 -std=c++17 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)	

main: librocksdb main.cc
	$(CXX) $(CXXFLAGS) $@.cc input.cc return.cc merge_manager.cc filter.cc csd_table_manager.cc scan.cc  -o$@ ../rocksdb/librocksdb.a -I../rocksdb/include -ldl -O2 -std=c++17 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)	

test: librocksdb test.cc
	$(CXX) $(CXXFLAGS) $@.cc filter.cc -o$@ ../rocksdb/librocksdb.a -I../rocksdb/include -ldl -O2 -std=c++17 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)	

main11: librocksdb main.cc
	$(CXX) $(CXXFLAGS) main.cc input.cc return.cc merge_manager.cc filter.cc csd_table_manager.cc scan.cc  -o$@ ../rocksdb/librocksdb.a -I../rocksdb/include -ldl -lastring -O2 -std=c++17 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)	

main111: librocksdb main.cc
	$(CXX) $(CXXFLAGS) main.cc input.cc return.cc merge_manager.cc filter.cc csd_table_manager.cc scan.cc  -o$@ ../rocksdb/librocksdb.a -I../rocksdb/include -ldl -larmpl -O2 -std=c++17 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)	

main8: librocksdb main.cc
	$(CXX) $(CXXFLAGS) main.cc input.cc return.cc merge_manager.cc filter.cc csd_table_manager.cc scan.cc  -o$@ ../rocksdb/librocksdb.a -I../rocksdb/include -ldl -lz -lbz2 -O2 -std=c++17 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)	
main811: librocksdb main.cc
	$(CXX) $(CXXFLAGS) main.cc input.cc return.cc merge_manager.cc filter.cc csd_table_manager.cc scan.cc  -o$@ ../rocksdb/librocksdb.a -I../rocksdb/include -ldl -lz -lbz2 -lastring -O2 -std=c++17 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)	

main8111: librocksdb main.cc
	$(CXX) $(CXXFLAGS) main.cc input.cc return.cc merge_manager.cc filter.cc csd_table_manager.cc scan.cc  -o$@ ../rocksdb/librocksdb.a -I../rocksdb/include -ldl -lz -lbz2 -larmpl -O2 -std=c++17 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)	

arm: armclang++ -fno-rtti main.cc input.cc return.cc merge_manager.cc filter.cc csd_table_manager.cc scan.cc  -oarmmainstring ../rocksdb/librocksdb.a -I../rocksdb/include -ldl  -O2 -std=c++17     -lpthread -armpl

arm8: armclang++ -fno-rtti main.cc input.cc return.cc merge_manager.cc filter.cc csd_table_manager.cc scan.cc  -oarmmainstring ../rocksdb/librocksdb.a -I../rocksdb/include -ldl -lz -lbz2 -O2 -std=c++17     -lpthread -armpl