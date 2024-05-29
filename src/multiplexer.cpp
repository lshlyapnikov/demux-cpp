#include "./multiplexer.h"
#include <span>

namespace ShmSequencer {

using std::span;
using std::uint8_t;

template <size_t N, uint16_t M>
  requires(N >= M + 2 && M > 0)
auto MultiplexerPublisher<N, M>::send_(const span<uint8_t> source) noexcept -> bool {
  const size_t n = source.size();
  if (n == 0) {
    return true;
  } else if (n > M) {
    return false;
  } else {
    // it either writes the entire message or nothing
    const size_t written = this->buffer_.write(this->position_, source);
    if (written > 0) {
      this->position_ += written;
      this->increment_message_count_();
      return true;
    } else {
      this->wait_for_subs_to_catch_up_and_wraparound_();
      // TODO: check recursion level, exit with error after it gets greater than 2
      return this->send_(source);
    }
  }
}

template <size_t N, uint16_t M>
  requires(N >= M + 2 && M > 0)
auto MultiplexerPublisher<N, M>::wait_for_subs_to_catch_up_and_wraparound_() const noexcept -> void {
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

template <size_t N, uint16_t M>
  requires(N >= M + 2 && M > 0)
auto MultiplexerSubscriber<N, M>::read() noexcept -> const span<uint8_t> {
  this->wait_for_message_count_increment_();
  const span<uint8_t>& result = this->buffer_.read(this->position_);
  this->read_message_count_ = +1;
  const size_t msg_size = result.size();
  if (msg_size > 0) {
    this->position_ += msg_size + sizeof(MessageBuffer::message_length_t);
    return result;
  } else {
    assert(this->read_message_count_ == this->available_message_count_);
    // signal that it is ready to wraparound, see doc/adr/ADR003.md for more details
    this->position_ = 0;
    this->wraparound_sync_->fetch_or(this->id_.mask());
    // TODO: check recursion level, exit with error after it gets greater than 2
    return this->read();
  }
}

template <size_t N, uint16_t M>
  requires(N >= M + 2 && M > 0)
auto MultiplexerSubscriber<N, M>::wait_for_message_count_increment_() noexcept -> void {
  if (this->read_message_count_ < this->available_message_count_) {
    return;
  } else {
    while (true) {
      this->message_count_sync_.wait(10);
      const uint64_t x = this->message_count_sync_->load();
      if (x > this->available_message_count_) {
        this->available_message_count_ = x;
        return;
      }
    }
  }
}

}  // namespace ShmSequencer