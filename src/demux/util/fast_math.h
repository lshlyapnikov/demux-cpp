// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#include <bit>
#include <cstddef>

#pragma once

namespace lshl::demux::util {
using std::size_t;

[[nodiscard]] constexpr auto is_power_of_2(const size_t x) noexcept -> bool {
  return (x > 0) && (std::bit_ceil(x) == x);
}

template <size_t N>
[[nodiscard]] constexpr auto fast_modulo(const size_t x) noexcept -> size_t {
  static_assert(is_power_of_2(N), "N must be a power of 2 for optimization");
  return x & (N - 1);
}
}  // namespace lshl::demux::util