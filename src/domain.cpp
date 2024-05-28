#include "./domain.h"

namespace ShmSequencer {
auto my_pow(uint8_t x, uint8_t p) -> uint64_t {
  if (p == 0) {
    return 1;
  }
  uint64_t result = x;
  for (size_t i = 2; i <= p; ++i) {
    result *= x;
  }
  return result;
}
}  // namespace ShmSequencer