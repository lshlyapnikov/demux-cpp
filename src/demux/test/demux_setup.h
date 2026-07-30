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
using std::span;
using std::uint64_t;
using std::uint8_t;
using std::vector;

template <size_t L, uint16_t M, bool B>
class DemuxSetup {
 private:
  array<uint8_t, L> buffer_;
  atomic<size_t> downstream_sequence_{0};
  vector<atomic<size_t>> upstream_sequences_;
  vector<shared_ptr<DemuxReader<L, M>>> readers_;
  DemuxWriter<L, M, B> writer_;

 public:
  explicit DemuxSetup(const uint8_t reader_num)
      : upstream_sequences_(reader_num),
        writer_(span<uint8_t, L>{buffer_}, &downstream_sequence_, to_ptrs(&upstream_sequences_)) {
    for (uint8_t i = 0; i < reader_num; ++i) {
      this->readers_.emplace_back(
          std::make_shared<DemuxReader<L, M>>(
              ReaderId{i}, span<uint8_t, L>{buffer_}, &downstream_sequence_, &upstream_sequences_[i]
          )
      );
    }
    assert(reader_num == this->readers_.size());
    assert(reader_num == this->upstream_sequences_.size());
  }

  [[nodiscard]] auto writer() -> DemuxWriter<L, M, B>* { return &writer_; }

  [[nodiscard]] auto reader(const size_t index) -> DemuxReader<L, M>* { return readers_.at(index).get(); }

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