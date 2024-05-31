#ifndef SHM_SEQUENCER_MULTIPLEXER_H
#define SHM_SEQUENCER_MULTIPLEXER_H
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include "atomic_util.h"
#include "domain.h"

namespace ShmSequencer {

using std::array;
using std::atomic;
using std::make_unique;
using std::nullopt;
using std::optional;
using std::size_t;
using std::span;
using std::uint64_t;
using std::uint8_t;
using std::unique_ptr;

/// @brief MessageBuffer wraps a byte array, which can be allocated in shared memory. MessageBuffer does not take the
/// ownership of the passed buffer. The outside logic is responsible to freeing the passed buffer. External
/// synchronization is required for accessing the `data_` via `read` and `write` calls. Read or write `position` must be
/// stored outside of this class.
struct MessageBuffer {
  using message_length_t = uint16_t;

 public:
  MessageBuffer(span<uint8_t> buffer) : size_(buffer.size()), data_(buffer.data()) {}

  /// @brief Writes the entire passed message into the buffer or nothing.
  /// @param position -- the zero-based byte offset at which the message should be written.
  /// @param message -- the bytes that should be written.
  /// @return the total number of written bytes (2 + message length) or zero.
  auto write(const size_t position, const span<uint8_t> message) noexcept -> size_t {
    const size_t x = sizeof(message_length_t);
    const size_t n = message.size();
    const size_t total_required = n + x;

    if (this->remaining(position) >= total_required) {
      uint8_t* data = this->data_;
      std::copy_n(&n, x, data + position);                  // write message length
      std::copy_n(message.data(), n, data + position + x);  // write message bytes
      return total_required;
    } else {
      return 0;
    }
  }

  /// @param position -- zero-based byte offset.
  /// @return the available number of byte at the provided position.
  auto remaining(const size_t position) const noexcept -> size_t {
    if (this->size_ <= position) {
      return 0;
    } else {
      return this->size_ - position;
    }
  }

  /// @brief Reads the message at the specified position. The behavior is undefined when wrong position is provided,
  ///        position has to point to a 2-byte length value that precedes the bytes data.
  /// @param position zero-based byte offset.
  /// @return the message at the provided position.
  auto read(const size_t position) const noexcept -> span<uint8_t> {
    message_length_t msg_size = 0;
    uint8_t* data = this->data_;
    data += position;
    std::copy_n(data, sizeof(message_length_t), &msg_size);
    data += sizeof(message_length_t);
    return span(data, msg_size);
  }

 private:
  size_t const size_;
  uint8_t* const data_;
};

// TODO: make sure the size of the shares memory is a multiple of the page size. Because the operating system
// TODO: performs mapping operations over whole pages. So, you don't waste memory.
//
/// @brief Multiplexer publisher.
/// @tparam N total buffer size in bytes.
/// @tparam M max message size in bytes.
template <const size_t N, const uint16_t M>
  requires(N >= M + 2 && M > 0)
class MultiplexerPublisher {
 public:
  MultiplexerPublisher(uint64_t all_subs_mask,
                       span<uint8_t, N> buffer,
                       atomic<uint64_t>* message_count_sync,
                       atomic<uint64_t>* wraparound_sync) noexcept
      : all_subs_mask_(all_subs_mask),
        buffer_(buffer),
        message_count_sync_(message_count_sync),
        wraparound_sync_(wraparound_sync) {}

  auto send(const span<uint8_t> source) noexcept(false) -> bool;

  // template <const uint16_t K>
  //   requires(K <= M && K > 0)
  // auto send_static(const span<uint8_t, K> source) noexcept -> bool {
  //   return this->send_(source);
  // }

  auto message_count() const noexcept -> uint64_t { return this->message_count_; }

#ifdef UNIT_TEST

  auto data() const noexcept -> span<uint8_t> { return span(this->buffers_.data(), this->position_); }

  auto position() const noexcept -> size_t { return this->position_; }

  auto all_subs_mask() const noexcept -> uint64_t { return this->all_subs_mask_; }

#endif  // UNIT_TEST

 private:
  auto send_(const span<uint8_t> source) noexcept -> bool;

  auto wait_for_subs_to_catch_up_and_wraparound_() noexcept -> void;

  auto increment_message_count_() noexcept -> void {
    this->message_count_ += 1;
    this->message_count_sync_->store(this->message_count_);
  }

  const uint64_t all_subs_mask_;

  size_t position_{0};
  uint64_t message_count_{0};
  MessageBuffer buffer_;
  atomic<uint64_t>* const message_count_sync_;
  atomic<uint64_t>* const wraparound_sync_;
};

/// @brief Multiplexer subscriber. Should be mapped into shared memory allocated by MultiplexerPublisher.
/// @tparam N total buffer size in bytes.
/// @tparam M max message size in bytes.
template <const size_t N, const uint16_t M>
  requires(N >= M + 2 && M > 0)
class MultiplexerSubscriber {
 public:
  MultiplexerSubscriber(const SubscriberId& subscriber_id,
                        span<uint8_t, N> buffer,
                        atomic<uint64_t>* message_count_sync,
                        atomic<uint64_t>* wraparound_sync) noexcept
      : id_(subscriber_id),
        buffer_(buffer),
        message_count_sync_(message_count_sync),
        wraparound_sync_(wraparound_sync) {}

  auto read() noexcept -> const span<uint8_t>;

  auto message_count() const noexcept -> uint64_t { return this->read_message_count_; }

#ifdef UNIT_TEST
  auto data() const noexcept -> span<uint8_t> { return span(this->buffers_.data(), this->position_); }

  auto position() const noexcept -> size_t { return this->position_; }

  auto mask() const noexcept -> uint64_t { return this->all_subs_mask_; }
#endif  // UNIT_TEST

 private:
  auto wait_for_message_count_increment_() noexcept -> void;

  const SubscriberId id_;

  size_t position_{0};
  uint64_t available_message_count_{0};
  uint64_t read_message_count_{0};
  MessageBuffer const buffer_;
  atomic<uint64_t>* const message_count_sync_;
  atomic<uint64_t>* const wraparound_sync_;
};

template <size_t N, uint16_t M>
  requires(N >= M + 2 && M > 0)
auto MultiplexerPublisher<N, M>::send(const span<uint8_t> source) noexcept(false) -> bool {
  const size_t k = source.size();
  if (0 < k && k <= M) {
    return this->send_(source);
  } else {
    throw std::invalid_argument(std::string("Must have 0 < source.size() <= M. Got source.size(): ") +
                                std::to_string(k) + ", M: " + std::to_string(M));
  }
}

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
auto MultiplexerPublisher<N, M>::wait_for_subs_to_catch_up_and_wraparound_() noexcept -> void {
  // see doc/adr/ADR003.md for more details
  this->wraparound_sync_->store(0);
  this->send_(span<uint8_t>{});
  this->increment_message_count_();
  while (true) {
    const uint64_t x = this->wraparound_sync_->load();
    if (x == this->all_subs_mask_) {
      break;
    }
  }
  this->position_ = 0;
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

#endif  // SHM_SEQUENCER_MULTIPLEXER_H