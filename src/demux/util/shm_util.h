// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace lshl::demux::util {

// names take up some space in the managed_shared_memory
constexpr std::size_t BOOST_IPC_INTERNAL_METADATA_SIZE = 512;

// Linux memory page size
constexpr std::size_t LINUX_PAGE_SIZE = 4096;

constexpr auto calculate_required_shared_mem_size(
    const std::size_t data_size,
    const std::size_t metadata_size,
    const std::size_t page_size
) noexcept -> std::size_t {
  // names take some space in the managed_shared_memory, this is why BOOST_IPC_INTERNAL_METADATA_SIZE is added
  // total shared memory size, should be  a multiple of the page size (4kB on Linux). Because the operating system
  // performs mapping operations over whole pages. So, you don't waste memory.
  const std::size_t quotient = (data_size + metadata_size) / page_size;
  const std::size_t reminder = (data_size + metadata_size) % page_size;
  if (reminder > 0) {
    return (quotient + 1) * page_size;
  } else {
    return quotient * page_size;
  }
}

template <typename T>
concept OutputStreamConcept = requires(T os) {
  { os << std::declval<std::string>() };  // Stream should support the << operator for strings
};

template <OutputStreamConcept OutputStream>
auto operator<<(OutputStream& os, const std::span<uint8_t>& xs) -> OutputStream& {
  os << "hex:" << std::hex << '[';
  bool first = true;
  for (const uint8_t x : xs) {
    if (first) {
      first = false;
    } else {
      os << ' ';
    }
    os << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(x);
  }
  os << ']' << std::dec;
  return os;
}

template <OutputStreamConcept OutputStream>
auto operator<<(OutputStream& os, const std::vector<uint8_t>& xs) -> OutputStream& {
  os << "hex:" << std::hex << '[';
  bool first = true;
  for (const uint8_t x : xs) {
    if (first) {
      first = false;
    } else {
      os << ' ';
    }
    os << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(x);
  }
  os << ']' << std::dec;
  return os;
}

template <OutputStreamConcept OutputStream, size_t M>
auto operator<<(OutputStream& os, const std::array<uint8_t, M>& xs) -> OutputStream& {
  std::array<uint8_t, M> ys{xs};
  return operator<<(os, std::span{ys});
}

}  // namespace lshl::demux::util
