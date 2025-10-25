// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner)

#include "../util/shm_util.h"
#include <gtest/gtest.h>
#include <atomic>
#include <cstddef>
#include <new>

#define UNIT_TEST

namespace lshl::demux::util {

using lshl::demux::util::calculate_required_shared_mem_size;
using std::size_t;

struct DummyCacheAlignedStruct {
  alignas(std::hardware_destructive_interference_size) std::uint64_t data;
};

static_assert(sizeof(DummyCacheAlignedStruct) == std::hardware_destructive_interference_size, "Alignment check failed");

struct CriticalData {
  // Member 1: Aligned to the start of a cache line
  alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> counter1;

  // Member 2: Guaranteed to start at least a full cache line away from Member 1
  alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> counter2;
};

static_assert(
    sizeof(CriticalData) >= 2 * std::hardware_destructive_interference_size,
    "CriticalData size is not sufficient to prevent false sharing"
);

TEST(LearningTest, CheckAlignAs) {
  EXPECT_EQ(sizeof(DummyCacheAlignedStruct), 64);
  EXPECT_EQ(sizeof(CriticalData), 128);
}

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
}  // namespace lshl::demux::util
// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)