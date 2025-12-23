// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

#include "../util/shm_manager.h"
#include <gtest/gtest.h>
#include <cstddef>
#include <new>

#define UNIT_TEST

namespace lshl::demux::util {

// using lshl::demux::util::calculate_required_shared_mem_size;
using std::size_t;

static_assert(
    sizeof(CacheLinePaddedAtomic<uint64_t>) == std::hardware_destructive_interference_size,
    "CacheLinePaddedAtomic<uint64_t> size check failed"
);

static_assert(
    sizeof(CacheLinePaddedAtomic<size_t>) == std::hardware_destructive_interference_size,
    "CacheLinePaddedAtomic<size_t> size check failed"
);

static_assert(
    sizeof(CacheLinePaddedAtomic<uint8_t>) == std::hardware_destructive_interference_size,
    "CacheLinePaddedAtomic<size_t> size check failed"
);

static_assert(
    sizeof(CacheLinePaddedArray<uint8_t, 2>) == std::hardware_destructive_interference_size,
    "CacheLinePaddedArray<uint8_t, 2> size check failed"
);

static_assert(
    sizeof(CacheLinePaddedArray<uint8_t, 32>) % std::hardware_destructive_interference_size == 0,
    "CacheLinePaddedArray<uint8_t, 32> size check failed"
);

TEST(ShmManagerTest, CheckAlignAs) {
  ASSERT_EQ(std::hardware_destructive_interference_size, 64);
  ASSERT_EQ(sizeof(CacheLinePaddedAtomic<uint64_t>), 64);
  ASSERT_EQ(sizeof(CacheLinePaddedAtomic<uint32_t>), 64);
  ASSERT_EQ(sizeof(CacheLinePaddedAtomic<uint8_t>), 64);
  ASSERT_EQ(sizeof(CacheLinePaddedArray<uint8_t, 32>), 64);
  ASSERT_EQ(sizeof(CacheLinePaddedArray<uint8_t, 32>), 64);
  ASSERT_EQ(sizeof(CacheLinePaddedArray<uint8_t, 64>), 64);
  ASSERT_EQ(sizeof(CacheLinePaddedArray<uint8_t, 2048>) % 64, 0);
}

TEST(ShmManagerTest, ConstantsTest) {
  ASSERT_EQ(lshl::demux::util::LINUX_PAGE_SIZE, 4096);
}

}  // namespace lshl::demux::util
// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)