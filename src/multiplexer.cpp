#include "./multiplexer.h"
#include <span>

namespace ShmSequencer {

using std::span;

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
      this->increment_message_count_();
      return true;
    } else {
      this->wait_all_subs_reached_end_of_buffer_and_wraparound_();
      // TODO: check recursion level, exit with error after it gets greater than 2
      return this->send_(source);
    }
  }
}

template <size_t N, uint16_t M>
  requires(N >= M + 2 && M > 0)
auto MultiplexerPublisher<N, M>::wait_all_subs_reached_end_of_buffer_and_wraparound_() const noexcept -> void {
  // TODO: implement wraparound with message counter increment
  // see doc/adr/ADR003.md for more details
  this->wraparound_sync_->store(0);
  this->send_(span<uint8_t>{});
  this->increment_message_count_();
  while (true) {
    const uint64_t x = this->wraparound_sync_.load();
    if (x == this->all_subs_mask_) {
      break;
    }
  }
  this->position_ = 0;
}

template <size_t N, uint16_t M>
  requires(N >= M + 2 && M > 0)
auto MultiplexerPublisher<N, M>::increment_message_count_() noexcept -> void {
  this->message_count_ += 1;
  this->message_count_sync_->store(this->message_count_);
}

}  // namespace ShmSequencer