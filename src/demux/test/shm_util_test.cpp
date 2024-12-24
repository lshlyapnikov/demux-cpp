// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner)

#define UNIT_TEST

#include "../util/shm_util.h"
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>

using lshl::demux::util::calculate_required_shared_mem_size;
using std::size_t;

TEST(ShmUtilTest, ConstantsTest) {
  ASSERT_EQ(lshl::demux::util::BOOST_IPC_INTERNAL_METADATA_SIZE, 512);
  ASSERT_EQ(lshl::demux::util::LINUX_PAGE_SIZE, 4096);
}

TEST(ShmUtilTest, CalculateRequiredSharedMemSize) {
  constexpr size_t actual0 = calculate_required_shared_mem_size(10, 3, 4);
  ASSERT_EQ(actual0, 16);

  constexpr size_t actual1 = calculate_required_shared_mem_size(10, 4, 3);
  ASSERT_EQ(actual1, 15);

  constexpr size_t actual3 = calculate_required_shared_mem_size(10, 4, 2);
  ASSERT_EQ(actual3, 14);
}

// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)