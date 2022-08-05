#include <sys/stat.h>
#include <algorithm>
#include <bitset>

#include "rocksdb/sst_file_reader.h"
#include "rocksdb/slice.h"
#include "rocksdb/iterator.h"
#include "rocksdb/cache.h"
#include "rocksdb/options.h"
#include "rocksdb/status.h"
#include "rocksdb/slice.h"

#include "filter.h"
#include "csd_table_manager.h"

using namespace std;
using namespace ROCKSDB_NAMESPACE;

extern WorkQueue<Snippet> ScanQueue;
extern WorkQueue<Result> FilterQueue;
extern WorkQueue<MergeResult> ReturnQueue;

class Scan{
    public:
        Scan(TableManager table_m){
            CSDTableManager_ = table_m;
        }
        

        void Scanning();
        void BlockScan(SstBlockReader* sstBlockReader_, BlockInfo* blockInfo, 
                        Snippet *snippet_, Result *scan_result);
        void EnQueueData(Result scan_result, Snippet snippet_);
        void Column_Filtering(Result scan_result, Snippet snippet_);
        void GetColumnOff(char* data, Snippet snippet_, int colindex, int &varcharflag);

    private:
        TableManager CSDTableManager_;
        unordered_map<string,int> newoffmap;
        unordered_map<string,int> newlenmap;
        // int dev_fd;

};