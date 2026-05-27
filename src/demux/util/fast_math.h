// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#include <cstddef>

#pragma once

namespace lshl::demux::util {
using std::size_t;

template <size_t N>
[[nodiscard]] constexpr auto fast_modulo(const size_t x) -> size_t {
  static_assert(N % 2 == 0, "N must be a power of 2 for optimization");
  return x & (N - 1);
}
}  // namespace lshl::demux::util