// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#ifndef LSHL_UTIL_SHM_UTIL_H
#define LSHL_UTIL_SHM_UTIL_H

#include <cstddef>

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

}  // namespace lshl::demux::util

#endif  // LSHL_UTIL_SHM_UTIL_H