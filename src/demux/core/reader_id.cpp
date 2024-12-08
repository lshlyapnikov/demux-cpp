// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#include "./subscriber_id.h"
#include <iostream>

namespace lshl::demux {

auto my_pow(uint8_t x, uint8_t p) -> uint64_t {
  if (p == 0) {
    return 1;
  }
  uint64_t result = x;
  for (size_t i = 2; i <= p; ++i) {
    result *= x;
  }
  return result;
}

auto operator<<(std::ostream& os, const SubscriberId& sub) -> std::ostream& {
  os << "SubscriberId{index_: " << sub.index_ << ", mask_: " << sub.mask_ << "}";
  return os;
}

}  // namespace lshl::demux