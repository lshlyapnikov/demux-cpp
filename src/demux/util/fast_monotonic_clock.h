// Copyright 2026 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <x86intrin.h>
#include <ctime>
#include "timestamp_util.h"

namespace lshl::demux::util {

// NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
class FastMonotonicClock {
 public:
  static void init() { get_instance().calibrate(); }

  // Hot path: Returns nanoseconds since the steady_clock epoch
  static auto now_ns() noexcept -> uint64_t {
    const auto& inst = get_instance();
    const uint64_t tcs = timestamp_counter();
    const uint64_t delta_tcs = tcs - inst.base_tsc_;
    const uint64_t delta_ns = (NANOS_PER_SECOND * delta_tcs) / inst.cpu_frequency_;  // tcs / (tcs/s)
    return inst.base_ns_ + delta_ns;
  }

 private:
  uint64_t base_ns_;
  uint64_t base_tsc_;
  uint64_t cpu_frequency_;  // tcs/s

  FastMonotonicClock() = default;

  // Singleton instance
  static auto get_instance() -> FastMonotonicClock& {
    static FastMonotonicClock instance;
    return instance;
  }

  void calibrate() {
    // Warm up the TSC to ensure it's out of low-power states
    for (int i = 0; i < 100; ++i) {
      _mm_pause();
    }

    const uint64_t ns0 = timestamp_ns(CLOCK_REALTIME);
    const uint64_t tcs0 = timestamp_counter();

    // 1s busy-wait
    while ((monotonic_timestamp_ns() - ns0) < SLEEP_TIMEOUT_NS) {
      _mm_pause();
    };

    const uint64_t ns1 = timestamp_ns(CLOCK_REALTIME);
    const uint64_t tcs1 = timestamp_counter();

    const uint64_t delta_ns = ns1 - ns0;
    const uint64_t delta_tsc = tcs1 - tcs0;

    cpu_frequency_ = delta_tsc * NANOS_PER_SECOND / delta_ns;
    base_ns_ = ns0;
    base_tsc_ = tcs0;

    LOG_INFO << "[calibrate] base_ns: " << base_ns_ << ", base_tsc: " << base_tsc_
             << ", cpu_frequency: " << cpu_frequency_;
  }
};
// NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

}  // namespace lshl::demux::util