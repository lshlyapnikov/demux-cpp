// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <x86intrin.h>
#include <cstdint>
#include <ctime>
#include <system_error>
#include "boost_log_util.h"

namespace lshl::demux::util {

constexpr uint64_t NANOS_PER_SECOND = 1'000'000'000;

constexpr uint64_t SLEEP_TIMEOUT_NS = 1'000'000'000;

using std::size_t;
using std::uint64_t;

/*
[[nodiscard]] inline auto timestamp_ns(clockid_t clock_id) noexcept(false) -> uint64_t {
  timespec ts{};

  // 1. Compiler Barrier: Prevents the compiler from moving code
  // from after this point to before this point.
  asm volatile("" ::: "memory");

  // 2. Hardware Barrier (lfence): Ensures all previous instructions
  // (especially loads) have completed and prevents subsequent
  // instructions from starting before this fence.
  _mm_lfence();

  if (clock_gettime(clock_id, &ts) == 0) [[likely]] {
    // 3. Second Hardware Barrier: Prevents the "work" we are about to
    // measure from starting until the clock has been read.
    _mm_lfence();

    // 4. Second Compiler Barrier
    asm volatile("" ::: "memory");

    return (static_cast<uint64_t>(ts.tv_sec) * NANOS_PER_SECOND) + static_cast<uint64_t>(ts.tv_nsec);
  } else {
    const int err = errno;
    LOG_ERROR << "Failed to get system clock time, system_error: " << std::system_category().message(err);
    return 0;
  }
}
*/

[[nodiscard]] inline auto timestamp_ns(clockid_t clock_id) noexcept(false) -> uint64_t {
  timespec ts{};

  if (clock_gettime(clock_id, &ts) == 0) [[likely]] {
    return (static_cast<uint64_t>(ts.tv_sec) * NANOS_PER_SECOND) + static_cast<uint64_t>(ts.tv_nsec);
  } else {
    const int err = errno;
    LOG_ERROR << "Failed to get system clock time, system_error: " << std::system_category().message(err);
    return 0;
  }
}

[[nodiscard]] inline auto monotonic_timestamp_ns() noexcept(false) -> uint64_t {
  return timestamp_ns(CLOCK_MONOTONIC);
}

[[nodiscard]] inline auto timestamp_counter() noexcept -> uint64_t {
  // __rdtscp returns the TSC and a Processor ID. It ensures all previous instructions have finished
  // before the timestamp is taken.
  unsigned int cpu_id{0};
  return __rdtscp(&cpu_id);
}

struct TscConverter {
 private:
  // NOLINTNEXTLINE(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
  static constexpr size_t FIXED_POINT_SHIFT = 32;

  /// Fixed-point multiplier (scaled by 2^32)
  uint64_t multiplier_fp_;

 public:
  explicit TscConverter(double ns_per_tick)
      : multiplier_fp_(static_cast<uint64_t>(ns_per_tick * (1ULL << TscConverter::FIXED_POINT_SHIFT))) {
    LOG_INFO << "[TscConverter] ns_per_tick: " << ns_per_tick << ", multiplier_fp: " << multiplier_fp_;
  }

  /// Convert ticks to ns using fixed-point math
  [[nodiscard]] auto ticks_to_ns(uint64_t ticks) const noexcept -> uint64_t {
    // Use __int128 to prevent overflow during multiplication
    unsigned __int128 total_ns = static_cast<unsigned __int128>(ticks) * multiplier_fp_;
    // Shift back to get nanoseconds
    return static_cast<uint64_t>(total_ns >> FIXED_POINT_SHIFT);
  }
};

[[nodiscard]] inline auto calibrate_best_effort() -> TscConverter {
  constexpr size_t SAMPLES = 101;
  constexpr uint64_t SLEEP_1MS = 1'000'000;
  std::vector<double> results;
  results.reserve(SAMPLES);

  for (size_t i = 0; i < SAMPLES; ++i) {
    _mm_lfence();
    const uint64_t tcs0 = timestamp_counter();
    const uint64_t ns0 = monotonic_timestamp_ns();

    // Busy wait 1ms per sample
    while (monotonic_timestamp_ns() - ns0 < SLEEP_1MS) {
    };

    const uint64_t ns1 = monotonic_timestamp_ns();
    const uint64_t tcs1 = timestamp_counter();
    _mm_lfence();

    const auto ns_delta = static_cast<double>(ns1 - ns0);
    const auto tcs_delta = static_cast<double>(tcs1 - tcs0);
    results.push_back(ns_delta / tcs_delta);
  }

  // Use the Median to filter out OS interrupts and context switches
  std::ranges::sort(results);
  double median_ns_per_tick = results[SAMPLES / 2];

  return TscConverter(median_ns_per_tick);
}

}  // namespace lshl::demux::util