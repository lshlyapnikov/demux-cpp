#ifndef SHM_SEQUENCER_ATOMIC_UTIL_H
#define SHM_SEQUENCER_ATOMIC_UTIL_H

#include <atomic>
#include <cstdint>

namespace ShmSequencer {

// multiple threads can wait for the update.
//
// consider using https://en.cppreference.com/w/cpp/atomic/atomic/wait
template <typename T>
[[nodiscard]] inline auto wait_new_value_and_acquire(const std::atomic<T>& actual, T old_val) -> T {
  while (true) {
    const uint64_t x = actual.load(std::memory_order_acquire);
    if (x != old_val) {
      return x;
    }
  }
}

// multiple threads can wait for the update.
//
// consider using https://en.cppreference.com/w/cpp/atomic/atomic/wait
template <typename T>
inline auto wait_exact_value_and_acquire(const std::atomic<T>& actual, T new_val) -> void {
  while (true) {
    const T x = actual.load(std::memory_order_acquire);
    if (x == new_val) {
      return;
    }
  }
}

template <typename T>
[[nodiscard]] inline auto acquire(const std::atomic<T>& actual) -> T {
  return actual.load(std::memory_order_acquire);
}

// Only 1 thread is expected to set it.
template <typename T>
inline auto set_and_release(std::atomic<T>* actual, T new_val) -> void {
  actual->store(new_val, std::memory_order_release);
}

}  // namespace ShmSequencer

#endif  // SHM_SEQUENCER_ATOMIC_UTIL_H