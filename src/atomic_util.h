#ifndef SHM_SEQUENCER_ATOMIC_UTIL_H
#define SHM_SEQUENCER_ATOMIC_UTIL_H

#include <atomic>
#include <cstdint>
#include "./domain.h"

using std::uint64_t;

namespace ShmSequencer {

// multiple threads can wait for the update.
//
// consider using https://en.cppreference.com/w/cpp/atomic/atomic/wait
// `wait` doesn't exist in GCC 9.4.0 C++20
[[nodiscard]] inline auto wait_and_acquire(const std::atomic<uint64_t>& actual, const uint64_t old) -> uint64_t {
  while (true) {
    const uint64_t x = actual.load(std::memory_order_acquire);
    if (x != old) {
      return x;
    }
  }
}

[[nodiscard]] inline auto acquire(const std::atomic<uint64_t>& actual) -> uint64_t {
  return actual.load(std::memory_order_acquire);
}

// Only 1 thread is expected to set it.
inline auto set_and_release(std::atomic<uint64_t>& actual, const uint64_t new_one) -> void {
  actual.store(new_one, std::memory_order_release);
}

}  // namespace ShmSequencer

#endif  // SHM_SEQUENCER_ATOMIC_UTIL_H