#include "csd_table_manager.h"

void TableManager::InitCSDTableManager(){
    // /dev/sda /dev/ngd-blk
    TableRep tr = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
    table_rep_.insert({"small_line.lineitem111_Table1",tr});
	TableRep tr2 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"tpch_100000.lineitem_100000",tr2});
    TableRep tr3 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"tpch_100000.part",tr3});
    TableRep tr4 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"tpch8test.part",tr4});
    TableRep tr5 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"tpch8test.lineitem_100000",tr5});
    TableRep tr6 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"tpch200.lineitem",tr6});
    TableRep tr7 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"tpch_line_part.lineitem_100000",tr7});
    TableRep tr8 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"tpch_line_part.part",tr8});
    TableRep tr9 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"tpch_line_part.customer",tr9});
    TableRep tr10 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"tpch_line_part.orders",tr10});
    TableRep tr11 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"tpch_line_part.partsupp",tr11});
    TableRep tr12 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"tpch_line_part.supplier",tr12});
    TableRep tr13 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"tpch_line_part.nation",tr13});
    TableRep tr14 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"tpch100.lineitem",tr14});
    //compression_name NoCompression
}

TableRep TableManager::GetTableRep(string table_name){
    // cout << "[Get Table Rep] \n #Table Name: '" << table_name << "'" << endl;	

    TableRep temp = table_rep_[table_name];
    // cout << "-------------------Table Rep--------------------"<<endl;
    // cout << "1. dev name: " << temp.dev_name << endl; 
    // cout << "2. blocks_maybe_compressed: " << temp.blocks_maybe_compressed << endl; 
    // cout << "3. blocks_definitely_zstd_compressed: " << temp.blocks_definitely_zstd_compressed << endl; 
    // // cout << "4. immortal_table: " << temp.immortal_table << endl; 
    // // cout << "5. read_amp_bytes_per_bit: " << temp.read_amp_bytes_per_bit << endl; 
    // cout << "4. compression_name: " << temp.compression_name << endl; 
    // cout << "----------------------------------------------------------------------------------"<<endl;

    // if (table_rep_.find(sstfile_name) == table_rep_.end()){
	// 	std::cout << "Not Present" << std::endl;
    //     return;
	// }

	return table_rep_[table_name];
}