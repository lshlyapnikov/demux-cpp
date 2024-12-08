// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#ifndef DEMUX_CPP_LSHL_DEMUX_DEMULTIPLEXER_H
#define DEMUX_CPP_LSHL_DEMUX_DEMULTIPLEXER_H

#include <atomic>
#include <boost/log/trivial.hpp>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include "./message_buffer.h"
#include "./subscriber_id.h"

namespace lshl::demux {

using std::atomic;
using std::size_t;
using std::span;
using std::uint64_t;
using std::uint8_t;

enum SendResult : std::uint8_t {
  Success,  // message sent
  Repeat,   // wraparound required, resend the last message
  Error,    // message cannot be sent, log error and drop it
};

/// @brief Demultiplexer publisher.
/// @tparam `L` The size of the circular buffer in bytes. When allocating the buffer in shared memory,
/// ensure that `L` is a multiple of the OS page size. This is because the Linux operating system
/// maps memory in whole pages, preventing memory waste.
/// @tparam `M` The maximum message size in bytes.
/// @tparam `B` If `true`, `send` will block/busy-spin while waiting for all subscribers to catch up during a
/// wraparound synchronization. If `false`, it will return immediately with `SendResult::Repeat`.
template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
class DemuxPublisher {
 public:
  DemuxPublisher(
      uint64_t all_subs_mask,
      span<uint8_t, L> buffer,
      atomic<uint64_t>* message_count_sync,
      atomic<uint64_t>* wraparound_sync
  ) noexcept
      : all_subs_mask_(all_subs_mask),
        buffer_(buffer),
        message_count_sync_(message_count_sync),
        wraparound_sync_(wraparound_sync) {
    BOOST_LOG_TRIVIAL(info) << "DemuxPublisher::constructor L: " << L << ", M: " << M << ", B: " << B
                            << ", all_subs_mask_: " << this->all_subs_mask_;
  }

  /// @brief Sends/copies the `source` into the buffer.
  /// @param `source` message that will be copied into the circular buffer.
  /// @return
  [[nodiscard]] auto send(const span<uint8_t>& source) noexcept -> SendResult {
    const size_t n = source.size();
    if (n == 0 || n > M) {
      BOOST_LOG_TRIVIAL(error) << "DemuxPublisher::send invalid message length: " << n;
      return SendResult::Error;
    }
    if constexpr (B) {
      return this->send_blocking_(source, 1);
    } else {
      return this->send_non_blocking_(source);
    }
  }

  /// @brief Serializes the `source` object of type `T` using `reinterpret_cast` and copies it into the circular
  /// buffer. `T` must be a simple, flat class without references to other objects; otherwise, behavior is
  /// unspecified. Consider using custom serialization/deserialization with `send` and `next` that take and return
  /// `span`.
  /// @see [reinterpret_cast documentation](https://en.cppreference.com/w/cpp/language/reinterpret_cast)
  /// @tparam `T`
  /// @param `source` message that will be copied into the circular buffer.
  /// @return
  template <class T>
    requires(sizeof(T) <= M)
  [[nodiscard]] auto send_object(const T& source) -> SendResult {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-type-const-cast, modernize-use-auto)
    uint8_t* x = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(&source));
    constexpr size_t X = sizeof(T);
    const span<uint8_t, X> raw{x, X};
    return this->send_safe<X>(raw);
  }

  /// @brief does not check the size of the `span`, it is checked at compiles time, see the `requires` clause.
  /// @tparam `N` the message size.
  /// @param `source` message that will be copied into the circular buffer.
  /// @return
  template <uint16_t N>
    requires(0 < N && N <= M)
  [[nodiscard]] auto send_safe(const span<uint8_t, N>& source) noexcept -> SendResult {
    if constexpr (B) {
      return this->send_blocking_(source, 1);
    } else {
      return this->send_non_blocking_(source);
    }
  }

  [[nodiscard]] auto message_count() const noexcept -> uint64_t { return this->message_count_; }

  [[nodiscard]] auto is_subscriber(const SubscriberId& id) const noexcept -> bool {
    return this->all_subs_mask_ & id.mask();
  }

  auto add_subscriber(const SubscriberId& id) noexcept -> void { this->all_subs_mask_ |= id.mask(); }

  auto remove_subscriber(const SubscriberId& id) noexcept -> void { this->all_subs_mask_ &= ~id.mask(); }

#ifdef UNIT_TEST

  auto data() const noexcept -> span<uint8_t> { return {this->buffers_.data(), this->position_}; }

  auto position() const noexcept -> size_t { return this->position_; }

  auto all_subs_mask() const noexcept -> uint64_t { return this->all_subs_mask_; }

#endif  // UNIT_TEST

 private:
  /// @brief Blocks/busy-spins while waiting for subscribers to catch up during a wraparound.
  /// @param `source` message to send.
  /// @param recursion_level.
  /// @return SendResult.
  [[nodiscard]] auto send_blocking_(const span<uint8_t>& source, uint8_t recursion_level) noexcept -> SendResult;

  /// @brief does not block while waiting for subscribers to catch up, returns `SendResult::Repeat` instead.
  /// @param `source` message to send.
  /// @return `SendResult`.
  [[nodiscard]] auto send_non_blocking_(const span<uint8_t>& source) noexcept -> SendResult;

  auto wait_for_subs_to_catch_up_and_wraparound_() noexcept -> void;

  inline auto initiate_wraparound_() noexcept -> void;

  inline auto complete_wraparound_() noexcept -> void;

  [[nodiscard]] auto all_subs_caught_up_() noexcept -> bool;

  auto increment_message_count_() noexcept -> void {
    this->message_count_ += 1;
    this->message_count_sync_->store(this->message_count_);
  }

  uint64_t all_subs_mask_;

  size_t position_{0};
  uint64_t message_count_{0};
  MessageBuffer<L> buffer_;
  bool wraparound_{false};
  atomic<uint64_t>* message_count_sync_;
  atomic<uint64_t>* wraparound_sync_;
};

/// @brief Demultiplexer subscriber. Should be mapped into shared memory allocated by DemuxPublisher.
/// @tparam `L` The size of the circular buffer in bytes.
/// @tparam `M` The max message size in bytes.
template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
class DemuxSubscriber {
 public:
  DemuxSubscriber(
      const SubscriberId& subscriber_id,
      span<uint8_t, L> buffer,
      atomic<uint64_t>* message_count_sync,
      atomic<uint64_t>* wraparound_sync
  ) noexcept
      : id_(subscriber_id),
        buffer_(buffer),
        message_count_sync_(message_count_sync),
        wraparound_sync_(wraparound_sync) {
    BOOST_LOG_TRIVIAL(info) << "DemuxSubscriber::constructor L: " << L << ", M: " << M << ", " << this->id_;
  }

  /// @brief Does not block. Calls `has_next`. Returns a `span` pointing to the object in the circular buffer.
  ///   Do not keep the reference to the returned `span` between `next` calls, the underlying bytes can be overriden
  ///   when circular buffer wraps around. Copy the content of the returned span if you need to keep the reference.
  /// @return message or empty span if no data available.
  [[nodiscard]] auto next() noexcept -> const span<uint8_t>;

  /// @brief Receives a reference to the next message in the circular buffer and deserializes the raw bytes into the
  /// object of type `T` using `reinterpret_cast`. `T` must be a simple, flat class without references to other
  /// objects; otherwise, behavior is unspecified. Consider using custom serialization/deserialization with `send`
  /// and `next` that take and return `span`.
  /// @see [reinterpret_cast documentation](https://en.cppreference.com/w/cpp/language/reinterpret_cast)
  /// @tparam T
  /// @return optional `T`.
  template <class T>
  [[nodiscard]] auto next_object() noexcept -> std::optional<const T*> {
    const span<uint8_t> raw = this->next();
    if (raw.empty()) {
      return std::nullopt;
    } else {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      const T* x = reinterpret_cast<const T*>(raw.data());
      return std::make_optional(std::move(x));
    }
  }

  [[nodiscard]] auto has_next() noexcept -> bool;

  [[nodiscard]] auto message_count() const noexcept -> uint64_t { return this->read_message_count_; }

#ifdef UNIT_TEST
  auto data() const noexcept -> span<uint8_t> { return {this->buffers_.data(), this->position_}; }

  auto position() const noexcept -> size_t { return this->position_; }

  auto mask() const noexcept -> uint64_t { return this->all_subs_mask_; }
#endif  // UNIT_TEST

 private:
  SubscriberId id_;

  size_t position_{0};
  uint64_t available_message_count_{0};
  uint64_t read_message_count_{0};
  MessageBuffer<L> buffer_;
  atomic<uint64_t>* message_count_sync_;
  atomic<uint64_t>* wraparound_sync_;
};

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
auto DemuxPublisher<L, M, B>::send_blocking_(const span<uint8_t>& source, uint8_t recursion_level) noexcept
    -> SendResult {
  // it either writes the entire message or nothing
  const size_t written = this->buffer_.write(this->position_, source);
  if (written > 0) {
    this->position_ += written;
    this->increment_message_count_();
    return SendResult::Success;
  } else {
    if (recursion_level > 1) {
      BOOST_LOG_TRIVIAL(error) << "DemuxPublisher::send_ recursion_level: " << recursion_level;
      return SendResult::Error;
    } else {
      this->wait_for_subs_to_catch_up_and_wraparound_();
      return this->send_blocking_(source, recursion_level + 1);
    }
  }
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
auto DemuxPublisher<L, M, B>::send_non_blocking_(const span<uint8_t>& source) noexcept -> SendResult {
  const size_t n = source.size();

  if (n == 0 || n > M) {
    BOOST_LOG_TRIVIAL(error) << "DemuxPublisher::send_ invalid message length: " << n;
    return SendResult::Error;
  }

  if (this->wraparound_) {
    if (this->all_subs_caught_up_()) {
      this->complete_wraparound_();
    } else {
      return SendResult::Repeat;
    }
  }

  // writes the entire message or nothing
  const size_t written = this->buffer_.write(this->position_, source);
  if (written > 0) {
    this->position_ += written;
    this->increment_message_count_();
    return SendResult::Success;
  } else {
    this->initiate_wraparound_();
    return SendResult::Repeat;
  }
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
auto DemuxPublisher<L, M, B>::wait_for_subs_to_catch_up_and_wraparound_() noexcept -> void {
  this->initiate_wraparound_();

#ifndef NDEBUG
  BOOST_LOG_TRIVIAL(debug) << "DemuxPublisher::wait_for_subs_to_catch_up_and_wraparound_, message_count_: "
                           << this->message_count_ << ", position_: " << this->position_ << " ... waiting ...";
#endif

  // busy-wait
  while (!this->all_subs_caught_up_()) {
  }

  this->complete_wraparound_();
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
inline auto DemuxPublisher<L, M, B>::initiate_wraparound_() noexcept -> void {
  // see doc/adr/ADR003.md for more details
  this->wraparound_ = true;
  this->wraparound_sync_->store(0);
  std::ignore = this->buffer_.write(this->position_, {});
  this->increment_message_count_();
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
inline auto DemuxPublisher<L, M, B>::complete_wraparound_() noexcept -> void {
  this->position_ = 0;
  this->wraparound_ = false;
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
inline auto DemuxPublisher<L, M, B>::all_subs_caught_up_() noexcept -> bool {
  const uint64_t x = this->wraparound_sync_->load();
  return x == this->all_subs_mask_;
}

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
// NOLINTNEXTLINE(readability-const-return-type)
[[nodiscard]] auto DemuxSubscriber<L, M>::next() noexcept -> const span<uint8_t> {
#ifndef NDEBUG
  BOOST_LOG_TRIVIAL(debug) << "DemuxSubscriber::next() " << this->id_
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
    BOOST_LOG_TRIVIAL(debug) << "DemuxSubscriber::next() continue, " << this->id_
                             << ", read_message_count_: " << this->read_message_count_
                             << ", available_message_count_: " << this->available_message_count_
                             << ", position_: " << this->position_;
#endif
    assert(this->position_ <= L);
    return result;
  } else {
#ifndef NDEBUG
    BOOST_LOG_TRIVIAL(debug) << "DemuxSubscriber::next() wrapping up, " << this->id_
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
[[nodiscard]] auto DemuxSubscriber<L, M>::has_next() noexcept -> bool {
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

}  // namespace lshl::demux

#endif  // DEMUX_CPP_LSHL_DEMUX_DEMULTIPLEXER_H