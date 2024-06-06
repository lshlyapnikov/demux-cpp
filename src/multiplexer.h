#ifndef SHM_SEQUENCER_MULTIPLEXER_H
#define SHM_SEQUENCER_MULTIPLEXER_H

#include <atomic>
#include <boost/log/trivial.hpp>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <span>
#include <tuple>
#include "./domain.h"
#include "./message_buffer.h"

namespace ShmSequencer {

using std::atomic;
using std::size_t;
using std::span;
using std::uint64_t;
using std::uint8_t;

enum SendResult {
  Success,  // message sent
  Repeat,   // wraparound required, resend the last message
  Error,    // message cannot be sent, log error and drop it
};

// TODO(Leo): make sure the size of the shares memory is a multiple of the page size. Because the operating system
// TODO(Leo): performs mapping operations over whole pages. So, you don't waste memory.
//
/// @brief Multiplexer publisher.
/// @tparam L total buffer size in bytes.
/// @tparam M max message size in bytes.
template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
class MultiplexerPublisher {
 public:
  MultiplexerPublisher(uint64_t all_subs_mask,
                       span<uint8_t, L> buffer,
                       atomic<uint64_t>* message_count_sync,
                       atomic<uint64_t>* wraparound_sync) noexcept
      : all_subs_mask_(all_subs_mask),
        buffer_(buffer),
        message_count_sync_(message_count_sync),
        wraparound_sync_(wraparound_sync) {
    BOOST_LOG_TRIVIAL(info) << "MultiplexerPublisher::constructor L: " << L << ", M: " << M
                            << ", all_subs_mask_: " << this->all_subs_mask_;
  }

  [[nodiscard]] auto send(const span<uint8_t>& source) noexcept -> bool { return this->send_(source, 1); }

  template <uint16_t N>
    requires(0 < N && N <= M)
  [[nodiscard]] auto send_safe(const span<uint8_t, N>& source) noexcept -> bool {
    return this->send_(source, 1);
  }

  [[nodiscard]] auto message_count() const noexcept -> uint64_t { return this->message_count_; }

#ifdef UNIT_TEST

  auto data() const noexcept -> span<uint8_t> { return {this->buffers_.data(), this->position_}; }

  auto position() const noexcept -> size_t { return this->position_; }

  auto all_subs_mask() const noexcept -> uint64_t { return this->all_subs_mask_; }

#endif  // UNIT_TEST

 private:
  /// @brief will block while waiting for subscribers to catch up before wrapping around.
  /// @param source -- message to send.
  /// @param recursion_level
  /// @return true if message was sent.
  [[nodiscard]] auto send_(const span<uint8_t>& source, uint8_t recursion_level) noexcept -> bool;

  auto wait_for_subs_to_catch_up_and_wraparound_() noexcept -> void;

  auto increment_message_count_() noexcept -> void {
    this->message_count_ += 1;
    this->message_count_sync_->store(this->message_count_);
  }

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  const uint64_t all_subs_mask_;

  size_t position_{0};
  uint64_t message_count_{0};
  MessageBuffer<L> buffer_;
  atomic<uint64_t>* message_count_sync_;
  atomic<uint64_t>* wraparound_sync_;
};

/// @brief Multiplexer subscriber. Should be mapped into shared memory allocated by MultiplexerPublisher.
/// @tparam L total buffer size in bytes.
/// @tparam M max message size in bytes.
template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
class MultiplexerSubscriber {
 public:
  MultiplexerSubscriber(const SubscriberId& subscriber_id,
                        span<uint8_t, L> buffer,
                        atomic<uint64_t>* message_count_sync,
                        atomic<uint64_t>* wraparound_sync) noexcept
      : id_(subscriber_id),
        buffer_(buffer),
        message_count_sync_(message_count_sync),
        wraparound_sync_(wraparound_sync) {
    BOOST_LOG_TRIVIAL(info) << "MultiplexerSubscriber::constructor L: " << L << ", M: " << M << ", " << this->id_;
  }

  /// @brief Does not block. Calls has_next.
  /// @return message or empty span if no data available.
  [[nodiscard]] auto next() noexcept -> const span<uint8_t>;

  [[nodiscard]] auto has_next() noexcept -> bool;

  [[nodiscard]] auto message_count() const noexcept -> uint64_t { return this->read_message_count_; }

#ifdef UNIT_TEST
  auto data() const noexcept -> span<uint8_t> { return {this->buffers_.data(), this->position_}; }

  auto position() const noexcept -> size_t { return this->position_; }

  auto mask() const noexcept -> uint64_t { return this->all_subs_mask_; }
#endif  // UNIT_TEST

 private:
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  const SubscriberId id_;

  size_t position_{0};
  uint64_t available_message_count_{0};
  uint64_t read_message_count_{0};
  MessageBuffer<L> buffer_;
  atomic<uint64_t>* message_count_sync_;
  atomic<uint64_t>* wraparound_sync_;
};

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
auto MultiplexerPublisher<L, M>::send_(const span<uint8_t>& source, uint8_t recursion_level) noexcept -> bool {
  const size_t n = source.size();

  if (n == 0 || n > M) {
    BOOST_LOG_TRIVIAL(error) << "MultiplexerPublisher::send_ invalid message length: " << n;
    return false;
  }

  // it either writes the entire message or nothing
  const size_t written = this->buffer_.write(this->position_, source);
  if (written > 0) {
    this->position_ += written;
    this->increment_message_count_();
    return true;
  } else {
    if (recursion_level > 1) {
      BOOST_LOG_TRIVIAL(error) << "MultiplexerPublisher::send_ recursion_level: " << recursion_level;
      return false;
    } else {
      this->wait_for_subs_to_catch_up_and_wraparound_();
      return this->send_(source, recursion_level + 1);
    }
  }
}

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
auto MultiplexerPublisher<L, M>::wait_for_subs_to_catch_up_and_wraparound_() noexcept -> void {
  // see doc/adr/ADR003.md for more details
  this->wraparound_sync_->store(0);
  std::ignore = this->buffer_.write(this->position_, span<uint8_t>{});
  this->increment_message_count_();

#ifndef NDEBUG
  BOOST_LOG_TRIVIAL(debug) << "MultiplexerPublisher::wait_for_subs_to_catch_up_and_wraparound_, message_count_: "
                           << this->message_count_ << ", position_: " << this->position_ << " ... waiting ...";
#endif

  // busy-wait
  while (true) {
    const uint64_t x = this->wraparound_sync_->load();
    if (x == this->all_subs_mask_) {
      break;
    }
  }

  // wait
  // this->wraparound_sync_->wait(this->all_subs_mask_);

  this->position_ = 0;
}

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
// NOLINTNEXTLINE(readability-const-return-type)
[[nodiscard]] auto MultiplexerSubscriber<L, M>::next() noexcept -> const span<uint8_t> {
#ifndef NDEBUG
  BOOST_LOG_TRIVIAL(debug) << "MultiplexerSubscriber::next() " << this->id_
                           << ", read_message_count_: " << this->read_message_count_
                           << ", position_: " << this->position_;
#endif

  if (!this->has_next()) {
    return {};
  }

  const span<uint8_t>& result = this->buffer_.read(this->position_);
  this->read_message_count_ += 1;
  const size_t msg_size = result.size();
  assert(msg_size <= M);

  if (msg_size > 0) {
    this->position_ += msg_size;
    this->position_ += sizeof(uint16_t);
#ifndef NDEBUG
    BOOST_LOG_TRIVIAL(debug) << "MultiplexerSubscriber::next() continue, " << this->id_
                             << ", read_message_count_: " << this->read_message_count_
                             << ", available_message_count_: " << this->available_message_count_
                             << ", position_: " << this->position_;
#endif
    assert(this->position_ <= L);
    return result;
  } else {
#ifndef NDEBUG
    BOOST_LOG_TRIVIAL(debug) << "MultiplexerSubscriber::next() wrapping up, " << this->id_
                             << ", read_message_count_: " << this->read_message_count_
                             << ", available_message_count_: " << this->available_message_count_
                             << ", position_: " << this->position_;
#endif
    // signal that it is ready to wraparound, see doc/adr/ADR003.md for more details
    assert(this->read_message_count_ == this->available_message_count_);
    this->position_ = 0;
    this->wraparound_sync_->fetch_or(this->id_.mask());
    return {};
  }
}

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
[[nodiscard]] auto MultiplexerSubscriber<L, M>::has_next() noexcept -> bool {
  if (this->read_message_count_ < this->available_message_count_) {
    return true;
  } else {
    const uint64_t x = this->message_count_sync_->load();
    if (x > this->available_message_count_) {
      this->available_message_count_ = x;
      return true;
    } else {
      return false;
    }
  }
}

}  // namespace ShmSequencer

#endif  // SHM_SEQUENCER_MULTIPLEXER_H