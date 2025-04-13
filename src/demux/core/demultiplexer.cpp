// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#include "./demultiplexer.h"
#include <vector>
#include "./reader_id.h"

namespace lshl::demux::core {

auto mask_to_reader_ids(const uint64_t value) -> std::vector<ReaderId> {
  std::vector<ReaderId> result;
  if (0 == value) {
    return result;
  }
  uint64_t mask = 1;
  for (uint8_t i = 1; i <= lshl::demux::core::MAX_READER_NUM; ++i) {
    if ((mask & value) > 0) {
      result.emplace_back(i);
    }
    mask = mask << 1;
  }
  return result;
}

}  // namespace lshl::demux::core