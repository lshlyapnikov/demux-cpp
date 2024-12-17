// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#include "./reader_id.h"
#include <iostream>

namespace lshl::demux::core {

auto operator<<(std::ostream& os, const ReaderId& x) -> std::ostream& {
  os << "ReaderId{value: " << static_cast<uint64_t>(x.value()) << ", mask: " << x.mask() << "}";
  return os;
}

}  // namespace lshl::demux::core