#ifndef __SHM_SEQUENCER_UTIL_H__
#define __SHM_SEQUENCER_UTIL_H__

#include <atomic>
#include "./domain.h"

namespace ShmSequencer {

/// @brief consider using https://en.cppreference.com/w/cpp/atomic/atomic/wait
/// @param actual
/// @param old
/// @return
inline auto wait_for_change(const std::atomic<uint64_t>& actual, const uint64_t old) -> uint64_t {
  while (true) {
    const uint64_t x = actual.load();
    if (x != old) {
      return x;
    }
  }
}

}  // namespace ShmSequencer

#endif  // __SHM_SEQUENCER_UTIL_H__