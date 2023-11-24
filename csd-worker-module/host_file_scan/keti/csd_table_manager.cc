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
    
    TableRep tr15 = {"/dev/ngd-blk3",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"lineitem",tr15});
    TableRep tr16 = {"/dev/ngd-blk3",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"customer",tr16});
    TableRep tr17 = {"/dev/ngd-blk3",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"nation",tr17});
    TableRep tr18 = {"/dev/ngd-blk2",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"orders",tr18});
    TableRep tr19 = {"/dev/ngd-blk3",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"part",tr19});
    TableRep tr20 = {"/dev/ngd-blk3",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"partsupp",tr20});
    TableRep tr21 = {"/dev/ngd-blk3",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"region",tr21});
    TableRep tr22 = {"/dev/ngd-blk3",false,false,false,0,"NoCompression"};//임시저장
	table_rep_.insert({"supplier",tr22});
    //compression_name NoCompression
}

TableRep TableManager::GetTableRep(string table_name){
    TableRep temp = table_rep_[table_name];
	return table_rep_[table_name];
}