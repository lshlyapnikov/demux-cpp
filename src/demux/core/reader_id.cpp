// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#include "./reader_id.h"
#include <iostream>
#include <vector>

namespace lshl::demux::core {

auto operator<<(std::ostream& os, const ReaderId& x) -> std::ostream& {
  os << "ReaderId{value: " << static_cast<uint64_t>(x.value()) << ", mask: " << x.mask() << "}";
  return os;
}

auto operator<<(std::ostream& os, const std::vector<ReaderId>& xs) -> std::ostream& {
  os << '[';
  bool first = true;
  for (const auto& x : xs) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    os << x;
  }
  os << ']';
  return os;
}

}  // namespace lshl::demux::core