// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory)
#include <thread>

#include "scan.h"

using namespace std;

// ---------------현재 사용하는 인자(compression, cache X)-----------
// bool blocks_maybe_compressed = false;
// bool blocks_definitely_zstd_compressed = false;
// uint32_t read_amp_bytes_per_bit = 0;
// const bool immortal_table = false;
// std::string dev_name = "/dev/sda";
// -----------------------------------------------------------------

WorkQueue<Snippet> ScanQueue;
WorkQueue<Result> FilterQueue;
WorkQueue<Result> MergeQueue;
WorkQueue<MergeResult> ReturnQueue;

int main(int argc, char **argv) {
    std::string filename = argv[1];
    const char *file_path = filename.c_str();

    TableManager CSDTableManager = TableManager();
    CSDTableManager.InitCSDTableManager();

    thread InputInterface = thread(&Input::InputSnippet, Input(file_path));
    // Scan scan = Scan(CSDTableManager);
    // scan.Scanning();
    thread ScanLayer = thread(&Scan::Scanning, Scan(CSDTableManager));
    thread FilterLayer1 = thread(&Filter::Filtering, Filter());
    thread FilterLayer2 = thread(&Filter::Filtering, Filter());
    // thread FilterLayer3 = thread(&Filter::Filtering, Filter());
    thread MergeLayer = thread(&MergeManager::Merging, MergeManager());
    thread ReturnInterface = thread(&Return::ReturnResult, Return());

    InputInterface.detach();
    ScanLayer.detach();
    FilterLayer1.detach();
    FilterLayer2.detach();
    // FilterLayer3.detach();
    MergeLayer.detach();
    ReturnInterface.detach();
    
    return 0;
}
