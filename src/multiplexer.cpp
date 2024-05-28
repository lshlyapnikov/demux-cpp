#include "./multiplexer.h"

namespace ShmSequencer {

template <size_t N, uint16_t M>
  requires(N >= M + 2 && M > 0)
inline auto MultiplexerPublisher<N, M>::send_(const span<uint8_t> source) noexcept -> bool {
  const size_t n = source.size();
  if (n == 0) {
    return true;
  } else if (n > M) {
    return false;
  } else {
    const size_t written = this->buffer_.write(this->position_, source);
    if (written > 0) {
      this->position_ += written;
      return true;
    } else {
      // TODO implement wraparound with sync
      return 0;
    }
  }
}

}  // namespace ShmSequencer