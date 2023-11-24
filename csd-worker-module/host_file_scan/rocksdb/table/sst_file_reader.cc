//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#ifndef ROCKSDB_LITE

#include "rocksdb/sst_file_reader.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <unistd.h>

#include "db/arena_wrapped_db_iter.h"
#include "db/db_iter.h"
#include "db/dbformat.h"
#include "file/random_access_file_reader.h"
#include "options/cf_options.h"
#include "rocksdb/env.h"
#include "rocksdb/file_system.h"
#include "table/get_context.h"
#include "table/table_builder.h"
#include "table/table_reader.h"
#include "table/format.h"//추가
#include "table/block_based/block_based_table_reader.h"


namespace ROCKSDB_NAMESPACE {

struct SstFileReader::Rep {
  Options options;
  EnvOptions soptions;
  ImmutableOptions ioptions;
  MutableCFOptions moptions;

  std::unique_ptr<TableReader> table_reader;

  Rep(const Options& opts)
      : options(opts),
        soptions(options),
        ioptions(options),
        moptions(ColumnFamilyOptions(options)) {}
};

SstFileReader::SstFileReader(const Options& options) : rep_(new Rep(options)) {}

SstFileReader::~SstFileReader() {}

/*printf 디버깅 추가*/
Status SstFileReader::Open(const std::string& file_path) {
  // printf("called SstFileReader::Open\n");
  
  auto r = rep_.get();
  Status s;
  uint64_t file_size = 0;
  std::unique_ptr<FSRandomAccessFile> file;
  std::unique_ptr<RandomAccessFileReader> file_reader;
  FileOptions fopts(r->soptions);
  const auto& fs = r->options.env->GetFileSystem();

  s = fs->GetFileSize(file_path, fopts.io_options, &file_size, nullptr);
  if (s.ok()) {
    s = fs->NewRandomAccessFile(file_path, fopts, &file, nullptr);
  }
  if (s.ok()) {
    file_reader.reset(new RandomAccessFileReader(std::move(file), file_path));
  }
  if (s.ok()) {
    TableReaderOptions t_opt(r->ioptions, r->moptions.prefix_extractor,
                             r->soptions, r->ioptions.internal_comparator);
    // Allow open file with global sequence number for backward compatibility.
    t_opt.largest_seqno = kMaxSequenceNumber;
    
    s = r->options.table_factory->NewTableReader(t_opt, std::move(file_reader),
                                                 file_size, &r->table_reader);
  }

  return s;
}

Iterator* SstFileReader::NewIterator(const ReadOptions& roptions) {
  auto r = rep_.get();
  auto sequence = roptions.snapshot != nullptr
                      ? roptions.snapshot->GetSequenceNumber()
                      : kMaxSequenceNumber;
  ArenaWrappedDBIter* res = new ArenaWrappedDBIter();
  res->Init(r->options.env, roptions, r->ioptions, r->moptions,
            nullptr /* version */, sequence,
            r->moptions.max_sequential_skip_in_iterations,
            0 /* version_number */, nullptr /* read_callback */,
            nullptr /* db_impl */, nullptr /* cfd */,
            true /* expose_blob_index */, false /* allow_refresh */);
  auto internal_iter = r->table_reader->NewIterator(
      res->GetReadOptions(), r->moptions.prefix_extractor.get(),
      res->GetArena(), false /* skip_filters */,
      TableReaderCaller::kSSTFileReader);
  res->SetIterUnderDBIter(internal_iter);
  return res;
}

std::shared_ptr<const TableProperties> SstFileReader::GetTableProperties()
    const {
  return rep_->table_reader->GetTableProperties();
}

Status SstFileReader::VerifyChecksum(const ReadOptions& read_options) {
  return rep_->table_reader->VerifyChecksum(read_options,
                                            TableReaderCaller::kSSTFileReader);
}

/*----------SstBlockReader----------*/

struct SstBlockReader::Rep {
  Options options;
  EnvOptions soptions;
  ImmutableOptions ioptions;
  MutableCFOptions moptions;

  std::unique_ptr<InternalIterator> datablock_iter;

  Rep(const Options& opts)
      : options(opts),
        soptions(options),
        ioptions(options),
        moptions(ColumnFamilyOptions(options)){}
};

struct SstBlockReader::SnippetInfo{
  bool blocks_maybe_compressed;
  bool blocks_definitely_zstd_compressed;
  const bool immortal_table;
  uint32_t read_amp_bytes_per_bit;
  std::string dev_name;

  SnippetInfo(bool _blocks_maybe_compressed, 
              bool _blocks_definitely_zstd_compressed,
              const bool _immortal_table,
              uint32_t _read_amp_bytes_per_bit,
              std::string _dev_name)
              : immortal_table(_immortal_table) {
                blocks_maybe_compressed = _blocks_maybe_compressed;
                blocks_definitely_zstd_compressed = _blocks_definitely_zstd_compressed;
                read_amp_bytes_per_bit = _read_amp_bytes_per_bit;
                dev_name = _dev_name;
              }
}; 

SstBlockReader::SstBlockReader(const Options& options,
                              bool _blocks_maybe_compressed,
                              bool _blocks_definitely_zstd_compressed,
                              const bool _immortal_table,
                              uint32_t _read_amp_bytes_per_bit,
                              std::string _dev_name)
                              : rep_(new Rep(options)),
                                snippetinfo_(new SnippetInfo(
                                   _blocks_maybe_compressed, 
                                   _blocks_definitely_zstd_compressed,
                                   _immortal_table,
                                   _read_amp_bytes_per_bit, 
                                   _dev_name)) {}
SstBlockReader::~SstBlockReader() {}

Status SstBlockReader::Open(BlockInfo* blockinfo_) {
  //printf("[func call] sst_file_reader.cc > SstBlockReader::Open\n");
  
  auto r = rep_.get();
  auto si = snippetinfo_.get();
  Status s;

  ReadOptions ro;
  s = BlockBasedTable::BlockOpen(
      si->blocks_maybe_compressed, si->blocks_definitely_zstd_compressed, 
      si->immortal_table, si->read_amp_bytes_per_bit,
      BlockType::kData, blockinfo_->block_offset, blockinfo_->block_size, 
      blockinfo_->block_id, si->dev_name, r->ioptions.internal_comparator, 
      r->ioptions.stats, &r->datablock_iter); 

  return s;
}

Iterator* SstBlockReader::NewIterator(const ReadOptions& roptions) {
  auto r = rep_.get();
  auto sequence = roptions.snapshot != nullptr
                      ? roptions.snapshot->GetSequenceNumber()
                      : kMaxSequenceNumber;
  ArenaWrappedDBIter* res = new ArenaWrappedDBIter();
  res->Init(r->options.env, roptions, r->ioptions, r->moptions,
            nullptr /* version */, sequence,
            r->moptions.max_sequential_skip_in_iterations,
            0 /* version_number */, nullptr /* read_callback */,
            nullptr /* db_impl */, nullptr /* cfd */,
            true /* expose_blob_index */, false /* allow_refresh */);
  auto internal_iter = r->datablock_iter.get();
  res->SetIterUnderDBIter(internal_iter);
  return res;

}


}  // namespace ROCKSDB_NAMESPACE

#endif  // !ROCKSDB_LITE
