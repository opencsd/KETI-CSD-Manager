#include "csd_table_manager.h"

void TableManager::InitCSDTableManager(){
    // /dev/sda /dev/ngd-blk

    table_rep.insert({{"1","lineitem"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/lineitem/001388.sst"});
    table_rep.insert({{"1","customer"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/customer/001174.sst"});
    table_rep.insert({{"1","orders"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/orders/001533.sst"});
    table_rep.insert({{"1","part"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/part/000895.sst"});
    table_rep.insert({{"1","supplier"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/supplier/000771.sst"});
    table_rep.insert({{"1","partsupp"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/partsupp/001019.sst"});
    // table_rep.insert({{"1","nation"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/nation/001159.sst"});
    // table_rep.insert({{"1","region"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/region/001143.sst"});
    table_rep.insert({{"1","nation"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/nation/001715.sst"});
    table_rep.insert({{"1","region"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/region/001699.sst"});

    table_rep.insert({{"2","lineitem"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/lineitem/001404.sst"});
    table_rep.insert({{"2","customer"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/customer/001190.sst"});
    table_rep.insert({{"2","orders"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/orders/001548.sst"});
    table_rep.insert({{"2","part"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/part/000911.sst"});
    table_rep.insert({{"2","supplier"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/supplier/000787.sst"});
    table_rep.insert({{"2","partsupp"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/partsupp/001035.sst"});

    table_rep.insert({{"3","lineitem"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/lineitem/001420.sst"});
    table_rep.insert({{"3","customer"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/customer/001205.sst"});
    table_rep.insert({{"3","orders"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/orders/001576.sst"});
    table_rep.insert({{"3","part"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/part/000926.sst"});
    table_rep.insert({{"3","supplier"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/supplier/000802.sst"});
    table_rep.insert({{"3","partsupp"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/partsupp/001050.sst"});

    table_rep.insert({{"4","lineitem"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/lineitem/001436.sst"});
    table_rep.insert({{"4","customer"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/customer/001221.sst"});
    table_rep.insert({{"4","orders"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/orders/001591.sst"});//
    table_rep.insert({{"4","part"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/part/000942.sst"});
    table_rep.insert({{"4","supplier"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/supplier/000818.sst"});
    table_rep.insert({{"4","partsupp"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/partsupp/001066.sst"});

    table_rep.insert({{"5","lineitem"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/lineitem/001452.sst"});
    table_rep.insert({{"5","customer"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/customer/001236.sst"});
    table_rep.insert({{"5","orders"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/orders/001607.sst"});//
    table_rep.insert({{"5","part"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/part/000957.sst"});
    table_rep.insert({{"5","supplier"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/supplier/000833.sst"});
    table_rep.insert({{"5","partsupp"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/partsupp/001081.sst"});

    table_rep.insert({{"6","lineitem"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/lineitem/001468.sst"});
    table_rep.insert({{"6","customer"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/customer/001252.sst"});
    table_rep.insert({{"6","orders"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/orders/001622.sst"});
    table_rep.insert({{"6","part"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/part/000973.sst"});
    table_rep.insert({{"6","supplier"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/supplier/000849.sst"});
    table_rep.insert({{"6","partsupp"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/partsupp/001097.sst"});

    table_rep.insert({{"7","lineitem"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/lineitem/001484.sst"});
    table_rep.insert({{"7","customer"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/customer/001267.sst"});
    table_rep.insert({{"7","orders"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/orders/001638.sst"});//
    table_rep.insert({{"7","part"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/part/000988.sst"});
    table_rep.insert({{"7","supplier"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/supplier/000864.sst"});
    table_rep.insert({{"7","partsupp"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/partsupp/001112.sst"});

    table_rep.insert({{"8","lineitem"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/lineitem/001490.sst"});
    table_rep.insert({{"8","customer"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/customer/001283.sst"});
    table_rep.insert({{"8","orders"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/orders/001653.sst"});//
    table_rep.insert({{"8","part"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/part/001004.sst"});
    table_rep.insert({{"8","supplier"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/supplier/000880.sst"});
    table_rep.insert({{"8","partsupp"},"/home/ngd/csd-worker-module/csd_file_scan/keti/sstfile/partsupp/001128.sst"});

}

string TableManager::GetTableRep(string csd_name, string table_name){
    string file_path = table_rep[make_pair(csd_name,table_name)];
	return file_path;
}