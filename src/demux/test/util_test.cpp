// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

#include <gtest/gtest.h>
#include <chrono>
#include <ctime>
#include <thread>
#include "../util/timestamp_util.h"

#define UNIT_TEST

namespace {
auto absolute_dif(const std::uint64_t a, const std::uint64_t b) -> std::uint64_t {
  return static_cast<std::uint64_t>(std::abs(static_cast<std::int64_t>(a) - static_cast<std::int64_t>(b)));
}
}  // namespace

TEST(UtilTest, TscCounter) {
  namespace util = lshl::demux::util;
  using std::uint64_t;

  constexpr uint64_t ACCURACY_NS = 150;

  const util::TscConverter conv = util::calibrate_best_effort();

  for (int i = 0; i < 10; ++i) {
    const uint64_t ns0 = util::timestamp_ns(CLOCK_MONOTONIC);
    const uint64_t tsc0 = util::timestamp_counter();

    std::this_thread::sleep_for(std::chrono::microseconds(1));

    const uint64_t ns1 = util::timestamp_ns(CLOCK_MONOTONIC);
    const uint64_t tsc1 = util::timestamp_counter();

    const uint64_t delta_ns = ns1 - ns0;
    const uint64_t delta_tsc_ns = conv.ticks_to_ns(tsc1 - tsc0);
    ASSERT_LT(absolute_dif(delta_ns, delta_tsc_ns), ACCURACY_NS);
  }
}

// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)