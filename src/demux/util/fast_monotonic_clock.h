// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <x86intrin.h>
#include <chrono>
#include <thread>

// TODO(Leonid): untested and unused! Remove if end up not using it!

// NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
class FastMonotonicClock {
 public:
  // Call this once at the start of your application
  static void init() { get_instance().calibrate(); }

  // Hot path: Returns nanoseconds since the steady_clock epoch
  static auto now_ns() noexcept -> uint64_t {
    const auto& inst = get_instance();

    // 1. Read the Time Stamp Counter
    uint64_t cycles = __rdtsc();

    // 2. Calculate cycles since calibration
    uint64_t delta_cycles = cycles - inst.base_tsc_;

    // 3. Fixed-point multiplication (Scale = 2^32)
    // Using __int128 prevents overflow before the shift
    unsigned __int128 scaled_ns = static_cast<unsigned __int128>(delta_cycles) * inst.ns_per_cycle_;

    // 4. Shift back and add to base nanoseconds
    return inst.base_ns_ + static_cast<uint64_t>(scaled_ns >> 32);
  }

 private:
  uint64_t base_ns_;
  uint64_t base_tsc_;
  uint64_t ns_per_cycle_;  // Ratio scaled by 2^32

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

    // Take initial samples
    const auto t1 = std::chrono::steady_clock::now();
    uint64_t c1 = __rdtsc();

    // Sleep to build a measurable delta (100ms is standard)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto t2 = std::chrono::steady_clock::now();
    uint64_t c2 = __rdtsc();

    const uint64_t t1_ns =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1.time_since_epoch()).count());
    const uint64_t t2_ns =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t2.time_since_epoch()).count());

    uint64_t d_ns = t2_ns - t1_ns;
    uint64_t d_cycles = (c2 - c1);

    // Calculate fixed-point multiplier: (ns / cycles) * 2^32
    double ratio = static_cast<double>(d_ns) / static_cast<double>(d_cycles);
    ns_per_cycle_ = static_cast<uint64_t>(ratio * (1ULL << 32));

    base_tsc_ = c1;
    base_ns_ = t1_ns;
  }
};
// NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)