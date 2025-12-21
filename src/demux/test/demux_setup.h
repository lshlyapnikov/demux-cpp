// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "../core/demux_reader.h"
#include "../core/demux_writer.h"

namespace lshl::demux::core::test {

using std::array;
using std::atomic;
using std::shared_ptr;
using std::vector;

template <class M, size_t N, bool B>
class DemuxSetup {
 private:
  array<M, N> buffer_;
  atomic<size_t> writer_position_{0};
  vector<atomic<size_t>> reader_positions_;
  vector<atomic<bool>> reader_active_flags_;
  vector<shared_ptr<DemuxReader<M, N, B>>> readers_;
  DemuxWriter<M, N, B> writer_;

 public:
  explicit DemuxSetup(const uint8_t reader_num)
      : reader_positions_(reader_num),
        reader_active_flags_(reader_num),
        writer_{&buffer_, &writer_position_, to_ptrs(&reader_positions_), to_ptrs(&reader_active_flags_)} {
    for (uint8_t i = 0; i < reader_num; ++i) {
      assert(0 == reader_positions_[i].load(std::memory_order_relaxed));
      // create and store a reader
      this->readers_.emplace_back(
          std::make_shared<DemuxReader<M, N, B>>(ReaderId{i}, &buffer_, &writer_position_, &reader_positions_[i])
      );
      // activate the reader by default
      reader_active_flags_.at(i).store(true, std::memory_order_relaxed);
    }
  }

  auto writer() -> DemuxWriter<M, N, B>* { return &writer_; }

  auto reader(const size_t index) -> DemuxReader<M, N, B>* { return readers_.at(index).get(); }

 private:
  template <class A>
  static auto to_ptrs(vector<atomic<A>>* as) -> vector<const atomic<A>*> {
    vector<const atomic<A>*> result;
    result.reserve(as->size());
    for (auto& a : *as) {
      result.push_back(&a);
    }
    return result;
  }
};

}  // namespace lshl::demux::core::test