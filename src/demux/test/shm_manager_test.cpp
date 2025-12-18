// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner)

#include <gtest/gtest.h>
#include <cstddef>
#include <new>
#include "../util/shm_manager.h"

#define UNIT_TEST

namespace lshl::demux::util {

// using lshl::demux::util::calculate_required_shared_mem_size;
using std::size_t;

static_assert(
    sizeof(CacheLinePaddedAtomicUint64) == std::hardware_destructive_interference_size,
    "CacheLinePaddedAtomicUint64 size check failed"
);

TEST(ShmUtilTest, CheckAlignAs) {
  EXPECT_EQ(sizeof(CacheLinePaddedAtomicUint64), 64);
}

TEST(ShmUtilTest, ConstantsTest) {
  ASSERT_EQ(lshl::demux::util::LINUX_PAGE_SIZE, 4096);
}

TEST(ShmUtilTest, ShmPrimitivesSizeTest) {
  ASSERT_EQ(64 + (3 * 64) + 128, sizeof(ShmPrimitives<3, 128>));
  ASSERT_EQ(64 + (3 * 64) + 65024, sizeof(ShmPrimitives<3, 65024>));
}
}  // namespace lshl::demux::util
// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)