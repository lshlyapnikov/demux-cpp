#ifndef __SHM_SEQUENCER_UTIL_H__
#define __SHM_SEQUENCER_UTIL_H__

#include <atomic>
#include "./domain.h"

namespace ShmSequencer {

inline auto busy_wait_sequence_number_increase(const std::atomic<uint64_t>& actual, const uint64_t processed)
    -> uint64_t {
  while (true) {
    const uint64_t x = actual.load();
    if (x > processed) {
      return x;
    }
  }
}

}  // namespace ShmSequencer

#endif  // __SHM_SEQUENCER_UTIL_H__