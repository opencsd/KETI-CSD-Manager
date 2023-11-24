//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/block_fetcher.h"

#include <cinttypes>
#include <string>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <unistd.h>

#include "logging/logging.h"
#include "memory/memory_allocator.h"
#include "monitoring/perf_context_imp.h"
#include "rocksdb/compression_type.h"
#include "rocksdb/env.h"
#include "table/block_based/block.h"
#include "table/block_based/block_based_table_reader.h"
#include "table/block_based/block_type.h"
#include "table/block_based/reader_common.h"
#include "table/format.h"
#include "table/persistent_cache_helper.h"
#include "util/compression.h"
#include "util/stop_watch.h"

namespace ROCKSDB_NAMESPACE {

inline void BlockFetcher::ProcessTrailerIfPresent() {
  if (footer_.GetBlockTrailerSize() > 0) {
    assert(footer_.GetBlockTrailerSize() == BlockBasedTable::kBlockTrailerSize);
    if (read_options_.verify_checksums) {
      io_status_ = status_to_io_status(VerifyBlockChecksum(
          footer_.checksum_type(), slice_.data(), block_size_,
          file_->file_name(), handle_.offset()));
    }
    compression_type_ =
        BlockBasedTable::GetBlockCompressionType(slice_.data(), block_size_);
  } else {
    // E.g. plain table or cuckoo table
    compression_type_ = kNoCompression;
  }
}

inline bool BlockFetcher::TryGetUncompressBlockFromPersistentCache() {
  if (cache_options_.persistent_cache &&
      !cache_options_.persistent_cache->IsCompressed()) {
    Status status = PersistentCacheHelper::LookupUncompressedPage(
        cache_options_, handle_, contents_);
    if (status.ok()) {
      // uncompressed page is found for the block handle
      return true;
    } else {
      // uncompressed page is not found
      if (ioptions_.logger && !status.IsNotFound()) {
        assert(!status.ok());
        ROCKS_LOG_INFO(ioptions_.logger,
                       "Error reading from persistent cache. %s",
                       status.ToString().c_str());
      }
    }
  }
  return false;
}

inline bool BlockFetcher::TryGetFromPrefetchBuffer() {
  if (prefetch_buffer_ != nullptr) {
    IOOptions opts;
    IOStatus io_s = file_->PrepareIOOptions(read_options_, opts);
    if (io_s.ok() &&
        prefetch_buffer_->TryReadFromCache(
            opts, file_, handle_.offset(), block_size_with_trailer_, &slice_,
            &io_s, read_options_.rate_limiter_priority, for_compaction_)) {
      ProcessTrailerIfPresent();
      if (!io_status_.ok()) {
        return true;
      }
      got_from_prefetch_buffer_ = true;
      used_buf_ = const_cast<char*>(slice_.data());
    } else if (!io_s.ok()) {
      io_status_ = io_s;
      return true;
    }
  }
  return got_from_prefetch_buffer_;
}

inline bool BlockFetcher::TryGetCompressedBlockFromPersistentCache() {
  if (cache_options_.persistent_cache &&
      cache_options_.persistent_cache->IsCompressed()) {
    // lookup uncompressed cache mode p-cache
    std::unique_ptr<char[]> raw_data;
    io_status_ = status_to_io_status(PersistentCacheHelper::LookupRawPage(
        cache_options_, handle_, &raw_data, block_size_with_trailer_));
    if (io_status_.ok()) {
      heap_buf_ = CacheAllocationPtr(raw_data.release());
      used_buf_ = heap_buf_.get();
      slice_ = Slice(heap_buf_.get(), block_size_);
      ProcessTrailerIfPresent();
      return true;
    } else if (!io_status_.IsNotFound() && ioptions_.logger) {
      assert(!io_status_.ok());
      ROCKS_LOG_INFO(ioptions_.logger,
                     "Error reading from persistent cache. %s",
                     io_status_.ToString().c_str());
    }
  }
  return false;
}

inline void BlockFetcher::PrepareBufferForBlockFromFile() {
  // cache miss read from device
  if ((do_uncompress_ || ioptions_.allow_mmap_reads) &&
      block_size_with_trailer_ < kDefaultStackBufferSize) {
    // If we've got a small enough hunk of data, read it in to the
    // trivially allocated stack buffer instead of needing a full malloc()
    //
    // `GetBlockContents()` cannot return this data as its lifetime is tied to
    // this `BlockFetcher`'s lifetime. That is fine because this is only used
    // in cases where we do not expect the `GetBlockContents()` result to be the
    // same buffer we are assigning here. If we guess incorrectly, there will be
    // a heap allocation and memcpy in `GetBlockContents()` to obtain the final
    // result. Considering we are eliding a heap allocation here by using the
    // stack buffer, the cost of guessing incorrectly here is one extra memcpy.
    //
    // When `do_uncompress_` is true, we expect the uncompression step will
    // allocate heap memory for the final result. However this expectation will
    // be wrong if the block turns out to already be uncompressed, which we
    // won't know for sure until after reading it.
    //
    // When `ioptions_.allow_mmap_reads` is true, we do not expect the file
    // reader to use the scratch buffer at all, but instead return a pointer
    // into the mapped memory. This expectation will be wrong when using a
    // file reader that does not implement mmap reads properly.
    used_buf_ = &stack_buf_[0];
  } else if (maybe_compressed_ && !do_uncompress_) {
    compressed_buf_ = AllocateBlock(block_size_with_trailer_,
                                    memory_allocator_compressed_);
    used_buf_ = compressed_buf_.get();
  } else {
    heap_buf_ =
        AllocateBlock(block_size_with_trailer_, memory_allocator_);
    used_buf_ = heap_buf_.get();
  }
}

inline void BlockFetcher::InsertCompressedBlockToPersistentCacheIfNeeded() {
  if (io_status_.ok() && read_options_.fill_cache &&
      cache_options_.persistent_cache &&
      cache_options_.persistent_cache->IsCompressed()) {
    // insert to raw cache
    PersistentCacheHelper::InsertRawPage(cache_options_, handle_, used_buf_,
                                         block_size_with_trailer_);
  }
}

inline void BlockFetcher::InsertUncompressedBlockToPersistentCacheIfNeeded() {
  if (io_status_.ok() && !got_from_prefetch_buffer_ &&
      read_options_.fill_cache && cache_options_.persistent_cache &&
      !cache_options_.persistent_cache->IsCompressed()) {
    // insert to uncompressed cache
    PersistentCacheHelper::InsertUncompressedPage(cache_options_, handle_,
                                                  *contents_);
  }
}

inline void BlockFetcher::CopyBufferToHeapBuf() {
  assert(used_buf_ != heap_buf_.get());
  heap_buf_ = AllocateBlock(block_size_with_trailer_, memory_allocator_);
  memcpy(heap_buf_.get(), used_buf_, block_size_with_trailer_);
#ifndef NDEBUG
  num_heap_buf_memcpy_++;
#endif
}


inline void BlockFetcher::CopyBufferToCompressedBuf() {
  assert(used_buf_ != compressed_buf_.get());
  compressed_buf_ = AllocateBlock(block_size_with_trailer_,
                                  memory_allocator_compressed_);
  memcpy(compressed_buf_.get(), used_buf_, block_size_with_trailer_);
#ifndef NDEBUG
  num_compressed_buf_memcpy_++;
#endif
}

// Entering this method means the block is not compressed or do not need to be
// uncompressed. The block can be in one of the following buffers:
// 1. prefetch buffer if prefetch is enabled and the block is prefetched before
// 2. stack_buf_ if block size is smaller than the stack_buf_ size and block
//    is not compressed
// 3. heap_buf_ if the block is not compressed
// 4. compressed_buf_ if the block is compressed
// 5. direct_io_buf_ if direct IO is enabled
// After this method, if the block is compressed, it should be in
// compressed_buf_, otherwise should be in heap_buf_.
inline void BlockFetcher::GetBlockContents() {
  // std::cout << "[func call] block_fetcher.cc > BlockFetcher::GetBlockContents" <<std::endl;
  if (slice_.data() != used_buf_) {
    // the slice content is not the buffer provided
    *contents_ = BlockContents(Slice(slice_.data(), block_size_));
  } else {
    // page can be either uncompressed or compressed, the buffer either stack
    // or heap provided. Refer to https://github.com/facebook/rocksdb/pull/4096
    if (got_from_prefetch_buffer_ || used_buf_ == &stack_buf_[0]) {
      CopyBufferToHeapBuf();
    } else if (used_buf_ == compressed_buf_.get()) {
      if (compression_type_ == kNoCompression &&
          memory_allocator_ != memory_allocator_compressed_) {
        CopyBufferToHeapBuf();
      } else {
        heap_buf_ = std::move(compressed_buf_);
      }
    } else if (direct_io_buf_.get() != nullptr) {
      if (compression_type_ == kNoCompression) {
        CopyBufferToHeapBuf();
      } else {
        CopyBufferToCompressedBuf();
        heap_buf_ = std::move(compressed_buf_);
      }
    }
    *contents_ = BlockContents(std::move(heap_buf_), block_size_);
  }
#ifndef NDEBUG
  contents_->is_raw_block = true;
#endif
}

IOStatus BlockFetcher::ReadBlockContents() {
  // std::cout << "block_fetcher.cc > BlockFetcher::ReadBlockContents" <<std::endl;
  if (TryGetUncompressBlockFromPersistentCache()) {
    compression_type_ = kNoCompression;
#ifndef NDEBUG
    contents_->is_raw_block = true;
#endif  // NDEBUG
    return IOStatus::OK();
  }
  if (TryGetFromPrefetchBuffer()) {
    if (!io_status_.ok()) {
      return io_status_;
    }
  } else if (!TryGetCompressedBlockFromPersistentCache()) {
    IOOptions opts;
    io_status_ = file_->PrepareIOOptions(read_options_, opts);
    // Actual file read
    if (io_status_.ok()) {
      if (file_->use_direct_io()) {
        PERF_TIMER_GUARD(block_read_time);
        io_status_ = file_->Read(
            opts, handle_.offset(), block_size_with_trailer_, &slice_, nullptr,
            &direct_io_buf_, read_options_.rate_limiter_priority);
        PERF_COUNTER_ADD(block_read_count, 1);
        used_buf_ = const_cast<char*>(slice_.data());
      } else {
        PrepareBufferForBlockFromFile();
        PERF_TIMER_GUARD(block_read_time);
        io_status_ = file_->Read(opts, handle_.offset(),
                                 block_size_with_trailer_, &slice_, used_buf_,
                                 nullptr, read_options_.rate_limiter_priority);
        PERF_COUNTER_ADD(block_read_count, 1);
#ifndef NDEBUG
        if (slice_.data() == &stack_buf_[0]) {
          num_stack_buf_memcpy_++;
        } else if (slice_.data() == heap_buf_.get()) {
          num_heap_buf_memcpy_++;
        } else if (slice_.data() == compressed_buf_.get()) {
          num_compressed_buf_memcpy_++;
        }
#endif
      }
    }

      // printf("\n----------------------------------------------------------------------------------------\n"); 
      // std::cout << "#block_buf : " << std::endl;
      // for(size_t j = 0; j < block_size_with_trailer_; j++) {
      //   printf("%02X ",used_buf_[j]);
      // }
      // printf("\n----------------------------------------------------------------------------------------\n"); 


    // TODO: introduce dedicated perf counter for range tombstones
    switch (block_type_) {
      case BlockType::kFilter:
        PERF_COUNTER_ADD(filter_block_read_count, 1);
        break;

      case BlockType::kCompressionDictionary:
        PERF_COUNTER_ADD(compression_dict_block_read_count, 1);
        break;

      case BlockType::kIndex:
        PERF_COUNTER_ADD(index_block_read_count, 1);
        break;

      // Nothing to do here as we don't have counters for the other types.
      default:
        break;
    }

    PERF_COUNTER_ADD(block_read_byte, block_size_with_trailer_);
    if (!io_status_.ok()) {
      return io_status_;
    }

    if (slice_.size() != block_size_with_trailer_) {
      return IOStatus::Corruption("truncated block read from " +
                                  file_->file_name() + " offset " +
                                  ToString(handle_.offset()) + ", expected " +
                                  ToString(block_size_with_trailer_) +
                                  " bytes, got " + ToString(slice_.size()));
    }

    ProcessTrailerIfPresent();
    if (io_status_.ok()) {
      InsertCompressedBlockToPersistentCacheIfNeeded();
    } else {
      return io_status_;
    }
  }

  if (do_uncompress_ && compression_type_ != kNoCompression) {
    PERF_TIMER_GUARD(block_decompress_time);
    // compressed page, uncompress, update cache
    UncompressionContext context(compression_type_);
    UncompressionInfo info(context, uncompression_dict_, compression_type_);
    io_status_ = status_to_io_status(UncompressBlockContents(
        info, slice_.data(), block_size_, contents_, footer_.format_version(),
        ioptions_, memory_allocator_));
#ifndef NDEBUG
    num_heap_buf_memcpy_++;
#endif
    compression_type_ = kNoCompression;
  } else {
    GetBlockContents();
  }

  InsertUncompressedBlockToPersistentCacheIfNeeded();

  return io_status_;
}

IOStatus BlockFetcher::ReadBlockContentsByOffset() {
  // std::cout << "[func call] block_fetcher.cc > BlockFetcher::ReadBlockContentsByOffset" << std::endl;
  
  if (TryGetUncompressBlockFromPersistentCache()) {
    compression_type_ = kNoCompression;
#ifndef NDEBUG
    contents_->is_raw_block = true;
#endif  // NDEBUG
    return IOStatus::OK();
  }

  uint64_t read_offset = handle_.offset();

  if (TryGetFromPrefetchBuffer()) {
    if (!io_status_.ok()) {
      return io_status_;
    }
  } else if (!TryGetCompressedBlockFromPersistentCache()) {
    IOOptions opts;
    io_status_ = file_->PrepareIOOptions(read_options_, opts);
    // Actual file read
    if (io_status_.ok()) {
      if (file_->use_direct_io()) {
        PERF_TIMER_GUARD(block_read_time);
        PrepareBufferForBlockFromFile();

        char* scratch = used_buf_;

        lseek(fd_,read_offset,SEEK_SET);
        int read_size = read(fd_, scratch, block_size_with_trailer_);
        slice_ = Slice(scratch,block_size_with_trailer_);

        
        // printf("\n----------------------------------------------------------------------------------------\n"); 
         std::cout << "#block read size : " << read_size << std::endl;
        // std::cout << "#block_buf : " << std::endl;
        // for(size_t j = 0; j < block_size_with_trailer_; j++) {
        //   printf("%d ",slice_.data()[j]);
        // }
        // printf("\n----------------------------------------------------------------------------------------\n\n"); 

       
      } else {
        //실제 돌렸을때 여기로 들어온거 확인
        
        PrepareBufferForBlockFromFile();
        PERF_TIMER_GUARD(block_read_time);

        char* scratch = used_buf_;

        lseek(fd_,read_offset,SEEK_SET);
        int read_size = read(fd_, scratch, block_size_with_trailer_);
        slice_ = Slice(scratch,block_size_with_trailer_);

       
        // printf("\n----------------------------------------------------------------------------------------\n"); 
        std::cout << "#block read size : " << read_size << std::endl;
        // std::cout << "#block_buf : " << std::endl;
        // for(size_t j = 0; j < block_size_with_trailer_; j++) {
        //   printf("%d ",slice_.data()[j]);
        // }
        // printf("\n----------------------------------------------------------------------------------------\n\n"); 

        
        PERF_COUNTER_ADD(block_read_count, 1);

#ifndef NDEBUG
        if (slice_.data() == &stack_buf_[0]) {
          num_stack_buf_memcpy_++;
        } else if (slice_.data() == heap_buf_.get()) {
          num_heap_buf_memcpy_++;
        } else if (slice_.data() == compressed_buf_.get()) {
          num_compressed_buf_memcpy_++;
        }
#endif
      }
    }

    // TODO: introduce dedicated perf counter for range tombstones
    switch (block_type_) {
      case BlockType::kFilter:
        PERF_COUNTER_ADD(filter_block_read_count, 1);
        break;

      case BlockType::kCompressionDictionary:
        PERF_COUNTER_ADD(compression_dict_block_read_count, 1);
        break;

      case BlockType::kIndex:
        PERF_COUNTER_ADD(index_block_read_count, 1);
        break;

      // Nothing to do here as we don't have counters for the other types.
      default:
        break;
    }

    PERF_COUNTER_ADD(block_read_byte, block_size_with_trailer_);
    if (!io_status_.ok()) {
      return io_status_;
    }

    if (slice_.size() != block_size_with_trailer_) {
      return IOStatus::Corruption("truncated block read from " +
                                  file_->file_name() + " offset " +
                                  ToString(handle_.offset()) + ", expected " +
                                  ToString(block_size_with_trailer_) +
                                  " bytes, got " + ToString(slice_.size()));
    }

    ProcessTrailerIfPresent();
    if (io_status_.ok()) {
      InsertCompressedBlockToPersistentCacheIfNeeded();
    } else {
      return io_status_;
    }
  }

  if (do_uncompress_ && compression_type_ != kNoCompression) {
    PERF_TIMER_GUARD(block_decompress_time);
    // compressed page, uncompress, update cache
    UncompressionContext context(compression_type_);
    UncompressionInfo info(context, uncompression_dict_, compression_type_);
    io_status_ = status_to_io_status(UncompressBlockContents(
        info, slice_.data(), block_size_, contents_, footer_.format_version(),
        ioptions_, memory_allocator_));
#ifndef NDEBUG
    num_heap_buf_memcpy_++;
#endif
    compression_type_ = kNoCompression;
  } else {
    GetBlockContents();
  }

  InsertUncompressedBlockToPersistentCacheIfNeeded();

  return io_status_;
}

//추가
IOStatus PbaBlockFetcher::ReadBlockContentsByPBA() {
  //std::cout << "[func call] block_fetcher.cc > PbaBlockFetcher::ReadBlockContentsByPBA" << std::endl;
  
  //   if (TryGetUncompressBlockFromPersistentCache()) {
  //     compression_type_ = kNoCompression;
  // #ifndef NDEBUG
  //     contents_->is_raw_block = true;
  // #endif  // NDEBUG
  //     return IOStatus::OK();
  //   }

  // int dev_fd = open(dev_name.c_str(),O_RDONLY);
  // off64_t ret = lseek64(dev_fd, begin_LBA*sector_size+handle_offset, SEEK_SET);
  // int len = read(dev_fd, scratch, block_size_with_trailer);
  // slice_ = Slice(scratch,block_size_with_trailer);

  //const char *file_path = "/usr/local/rocksdb/000281.sst";

  //PrepareBufferForBlockFromFile();
  memset(internal_buf,0,sizeof(internal_buf));
	//used_buf_ = internal_buf;

  //char* scratch = used_buf_;

  //char buf[block_size_with_trailer];
  // std::cout << "#dev_name: " << dev_name.c_str() <<"| handle_offset : "<< block_offset << "| block_size_with_trailer: " << block_size_with_trailer << std::endl;
  // int dev_fd = open(dev_name.c_str(), O_RDONLY);
  // lseek(dev_fd,block_offset,SEEK_SET);
  // int read_size = read(dev_fd, scratch, block_size_with_trailer);
  // slice_ = Slice(scratch,block_size_with_trailer);
  // std::cout << "#buffer_read_size : " << read_size << std::endl;

  // close(dev_fd);

  //std::cout << "#dev_name: " << dev_name.c_str() <<"| handle_offset : "<< block_offset << "| block_size_with_trailer: " << block_size_with_trailer << std::endl;
  int dev_fd = open(dev_name.c_str(), O_RDONLY);
  lseek(dev_fd,block_offset,SEEK_SET);
  uint64_t read_size = read(dev_fd, internal_buf, block_size_with_trailer);
  if(read_size != block_size_with_trailer){
    std::cout << "error" << std::endl;
  }
  slice_ = Slice(internal_buf,block_size_with_trailer);
  //std::cout << "#buffer_read_size : " << read_size << std::endl;
  *contents_ = BlockContents(Slice(slice_.data(), block_size));

  close(dev_fd);

  // char data[4096] = {0};

  // std::cout << "#dev_name: " << dev_name.c_str() <<"| handle_offset : "<< block_offset << "| block_size_with_trailer: " << block_size_with_trailer << std::endl;
  // int dev_fd = open(dev_name.c_str(), O_RDONLY);
  // lseek(dev_fd,block_offset,SEEK_SET);
  // int read_size = read(dev_fd, data, block_size_with_trailer);
  // slice_ = Slice(data,block_size_with_trailer);
  // std::cout << "#buffer_read_size : " << read_size << std::endl;
  // *contents_ = BlockContents(Slice(slice_.data(), block_size));

  // close(dev_fd);

//   if (TryGetFromPrefetchBuffer()) {
//     if (!io_status_.ok()) {
//       return io_status_;
//     }
//   } else if (!TryGetCompressedBlockFromPersistentCache()) {
//     IOOptions opts;
//     io_status_ = file_->PrepareIOOptions(read_options_, opts);
//     // Actual file read
//     if (io_status_.ok()) {
//       if (file_->use_direct_io()) {
//         PERF_TIMER_GUARD(block_read_time);
//         PrepareBufferForBlockFromFile();

//         char* scratch = used_buf_;

//         lseek(fd_,read_offset,SEEK_SET);
//         int read_size = read(fd_, scratch, block_size_with_trailer_);
//         slice_ = Slice(scratch,block_size_with_trailer_);

        
//         // printf("\n----------------------------------------------------------------------------------------\n"); 
//          std::cout << "#block read size : " << read_size << std::endl;
//         // std::cout << "#block_buf : " << std::endl;
//         // for(size_t j = 0; j < block_size_with_trailer_; j++) {
//         //   printf("%d ",slice_.data()[j]);
//         // }
//         // printf("\n----------------------------------------------------------------------------------------\n\n"); 

       
//       } else {
//         //실제 돌렸을때 여기로 들어온거 확인
        
//         PrepareBufferForBlockFromFile();
//         PERF_TIMER_GUARD(block_read_time);

//         char* scratch = used_buf_;

//         lseek(fd_,read_offset,SEEK_SET);
//         int read_size = read(fd_, scratch, block_size_with_trailer_);
//         slice_ = Slice(scratch,block_size_with_trailer_);

       
//         // printf("\n----------------------------------------------------------------------------------------\n"); 
//         std::cout << "#block read size : " << read_size << std::endl;
//         // std::cout << "#block_buf : " << std::endl;
//         // for(size_t j = 0; j < block_size_with_trailer_; j++) {
//         //   printf("%d ",slice_.data()[j]);
//         // }
//         // printf("\n----------------------------------------------------------------------------------------\n\n"); 

        
//         PERF_COUNTER_ADD(block_read_count, 1);

// #ifndef NDEBUG
//         if (slice_.data() == &stack_buf_[0]) {
//           num_stack_buf_memcpy_++;
//         } else if (slice_.data() == heap_buf_.get()) {
//           num_heap_buf_memcpy_++;
//         } else if (slice_.data() == compressed_buf_.get()) {
//           num_compressed_buf_memcpy_++;
//         }
// #endif
//       }
//     }

    // // TODO: introduce dedicated perf counter for range tombstones
    // switch (block_type_) {
    //   case BlockType::kFilter:
    //     PERF_COUNTER_ADD(filter_block_read_count, 1);
    //     break;

    //   case BlockType::kCompressionDictionary:
    //     PERF_COUNTER_ADD(compression_dict_block_read_count, 1);
    //     break;

    //   case BlockType::kIndex:
    //     PERF_COUNTER_ADD(index_block_read_count, 1);
    //     break;

    //   // Nothing to do here as we don't have counters for the other types.
    //   default:
    //     break;
    // }

    // PERF_COUNTER_ADD(block_read_byte, block_size_with_trailer);
    // if (!io_status_.ok()) {
    //   return io_status_;
    // }

    if (slice_.size() != block_size_with_trailer) {
      std::cout << "slice_.size() != block_size_with_trailer" << std::endl;
    }

    //ProcessTrailerIfPresent();
    // if (io_status_.ok()) {
    //   InsertCompressedBlockToPersistentCacheIfNeeded();
    // } else {
    //   return io_status_;
    // }
  //}

//   if (do_uncompress_ && compression_type_ != kNoCompression) {
//     PERF_TIMER_GUARD(block_decompress_time);
//     // compressed page, uncompress, update cache
//     UncompressionContext context(compression_type_);
//     UncompressionInfo info(context, uncompression_dict_, compression_type_);
//     io_status_ = status_to_io_status(UncompressBlockContents(
//         info, slice_.data(), block_size_, contents_, footer_.format_version(),
//         ioptions_, memory_allocator_));
// #ifndef NDEBUG
//     num_heap_buf_memcpy_++;
// #endif
//     compression_type_ = kNoCompression;
//   } else {
     //GetBlockContentsByPBA();
//   }

//  InsertUncompressedBlockToPersistentCacheIfNeeded();


  // printf("\n----------------------------------------------------------------------------------------\n"); 
  // std::cout << "#contents : " << read_size << std::endl;
  // for(size_t j = 0; j < contents_->data.size(); j++) {
  //   printf("%d ",contents_->data[j]);
  // }
  // printf("\n----------------------------------------------------------------------------------------\n\n"); 



  return io_status_;
}

// inline void PbaBlockFetcher::GetBlockContentsByPBA() {
//   //std::cout << "[func call] block_fetcher.cc > PbaBlockFetcher::GetBlockContentsByPBA" <<std::endl;

//   *contents_ = BlockContents(std::move(heap_buf_), block_size);
// #ifndef NDEBUG
//   contents_->is_raw_block = true;
// #endif
// }

inline void PbaBlockFetcher::PrepareBufferForBlockFromFile() {
  // cache miss read from device
  // if ((do_uncompress_ || ioptions_.allow_mmap_reads) &&
  //     block_size_with_trailer_ < kDefaultStackBufferSize) {
  //   used_buf_ = &stack_buf_[0];
  // } else if (maybe_compressed_ && !do_uncompress_) {
  //   compressed_buf_ = AllocateBlock(block_size_with_trailer_,
  //                                   memory_allocator_compressed_);
  //   used_buf_ = compressed_buf_.get();

	// } else {
    //heap_buf_ =
        //AllocateBlock(block_size_with_trailer, memory_allocator_);
    //used_buf_ = heap_buf_.get();
	memset(internal_buf,0,sizeof(internal_buf));
	used_buf_ = internal_buf;
  //}
}


inline void PbaBlockFetcher::CopyBufferToHeapBuf() {
  assert(used_buf_ != heap_buf_.get());
  heap_buf_ = AllocateBlock(block_size_with_trailer, memory_allocator_);
  memcpy(heap_buf_.get(), used_buf_, block_size_with_trailer);
#ifndef NDEBUG
  num_heap_buf_memcpy_++;
#endif
}

// inline void PbaBlockFetcher::ProcessTrailerIfPresent() {
//   if (footer_block_trailer_size > 0) {
//     assert(footer_block_trailer_size == BlockBasedTable::kBlockTrailerSize);
//     if (read_options_.verify_checksums) {
//       io_status_ = status_to_io_status(VerifyBlockChecksum(
//           footer_checksum_type, slice_.data(), block_size,
//           dev_name, handle_offset));
//     }
//     compression_type_ =
//         BlockBasedTable::GetBlockCompressionType(slice_.data(), block_size);
//   } else {
//     // E.g. plain table or cuckoo table
//     compression_type_ = kNoCompression;
//   }
// }

}  // namespace ROCKSDB_NAMESPACE
