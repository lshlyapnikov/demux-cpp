// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <emmintrin.h>
#include <atomic>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>
#include "../util/boost_log_util.h"
#include "../util/fast_math.h"
#include "./message_buffer.h"
#include "./reader_id.h"

namespace lshl::demux::core {

using std::atomic;
using std::size_t;
using std::span;
using std::uint64_t;
using std::uint8_t;
using std::vector;

using lshl::demux::util::is_power_of_2;

enum WriteResult : std::uint8_t {
  Success,  // message sent
  Repeat,   // wraparound required, resend the last message
  Error,    // message cannot be sent, log error and drop it
};

/// @brief Demultiplexer writer.
/// @tparam `L` The size of the circular buffer in bytes. When allocating the buffer in shared memory,
/// ensure that `L` is a multiple of the OS page size. This is because the Linux operating system
/// maps memory in whole pages, preventing memory waste.
/// @tparam `M` The maximum message size in bytes.
/// @tparam `B` If `true`, `write` will block/busy-spin while waiting for all readers to catch up during a
/// wraparound synchronization. If `false`, it will return immediately with `WriteResult::Repeat`.
template <size_t L, uint16_t M, bool B>
class DemuxWriter {
  static_assert(L >= M + 2, "Buffer size L must be at least M + 2");
  static_assert(M > 0, "M must be greater than 0");
  static_assert(is_power_of_2(L), "Buffer size L must be a power of 2");

 private:
  size_t position_{0};
  uint64_t message_count_{0};
  MessageBuffer<L> buffer_;
  bool wraparound_{false};
  atomic<uint64_t>* downstream_sequence_;
  vector<const atomic<uint64_t>*> upstream_sequences_;

 public:
  DemuxWriter(
      span<uint8_t, L> buffer,
      atomic<uint64_t>* downstream_sequence,
      const vector<const atomic<uint64_t>*>& upstream_sequences
  ) noexcept
      : buffer_(buffer), downstream_sequence_(downstream_sequence), upstream_sequences_(upstream_sequences) {
    LOG_INFO << "[DemuxWriter::constructor] L: " << L << ", M: " << M << ", B: " << B
             << ", reader_num: " << this->upstream_sequences_.size();
  }

  ~DemuxWriter() = default;
  DemuxWriter(const DemuxWriter&) = delete;
  auto operator=(const DemuxWriter&) -> DemuxWriter& = delete;
  DemuxWriter(DemuxWriter&&) = default;
  auto operator=(DemuxWriter&&) -> DemuxWriter& = delete;

  /// @brief Writes/copies the `source` into the buffer.
  /// @param `source` message that will be copied into the circular buffer.
  /// @return
  [[nodiscard]] auto write(const span<uint8_t>& source) noexcept -> WriteResult {
    const size_t n = source.size();
    if (n == 0 || n > M) {
      LOG_ERROR << "[DemuxWriter::write] invalid message length: " << n;
      return WriteResult::Error;
    }
    if constexpr (B) {
      return this->write_blocking(source, 1);
    } else {
      return this->write_non_blocking(source);
    }
  }

  /// @brief Serializes the `source` object of type `T` using `reinterpret_cast` and copies it into the circular
  /// buffer. `T` must be a simple, flat class without references to other objects; otherwise, behavior is
  /// unspecified. Consider using custom serialization/deserialization with `write` and `next` that take and return
  /// `span`. This is a safe operation because `reinterpret_cast<const uint8_t*>(&source)` is well defined,
  /// it is casting to a byte pointer.
  /// @see [reinterpret_cast documentation](https://en.cppreference.com/w/cpp/language/reinterpret_cast)
  /// @tparam `T`
  /// @param `source` message that will be copied into the circular buffer.
  /// @return
  template <class T>
    requires(sizeof(T) <= M && sizeof(T) != 0)
  [[nodiscard]] auto write_safe(const T& source) -> WriteResult {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-type-const-cast, modernize-use-auto)
    uint8_t* x = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(&source));
    constexpr size_t X = sizeof(T);
    const span<uint8_t, X> raw{x, X};
    return this->write_safe<X>(raw);
  }

  template <class A>
    requires(std::default_initializable<A> && sizeof(A) != 0 && sizeof(A) <= M)
  [[nodiscard]] auto allocate() noexcept -> std::optional<A*> {
    if constexpr (B) {
      return this->allocate_blocking<A>(1);
    } else {
      return this->allocate_non_blocking<A>();
    }
  }

  template <class A>
    requires(sizeof(A) <= M && sizeof(A) != 0)
  auto commit() noexcept -> void {
    this->position_ += MessageBuffer<0>::required<A>();
    this->increment_message_count();
  }

  /// @brief does not check the size of the `span`, it is checked at compiles time, see the `requires` clause.
  /// @tparam `N` the message size.
  /// @param `source` message that will be copied into the circular buffer.
  /// @return
  template <uint16_t N>
    requires(0 < N && N <= M)
  [[nodiscard]] auto write_safe(const span<uint8_t, N>& source) noexcept -> WriteResult {
    if constexpr (B) {
      return this->write_blocking(source, 1);
    } else {
      return this->write_non_blocking(source);
    }
  }

  [[nodiscard]] auto message_count() const noexcept -> uint64_t { return this->message_count_; }

  [[nodiscard]] auto downstream_sequence() const noexcept -> uint64_t {
    return this->downstream_sequence_->load(std::memory_order_relaxed);
  }

  auto upstream_sequences(vector<uint64_t>* result) const noexcept -> void {
    result->clear();
    for (const atomic<uint64_t>* x : this->upstream_sequences_) {
      result->push_back(x->load(std::memory_order_relaxed));
    }
  }

#ifdef UNIT_TEST

  auto position() const noexcept -> size_t { return this->position_; }

#endif  // UNIT_TEST

 private:
  /// @brief Blocks/busy-spins while waiting for readers to catch up during a wraparound.
  /// @param `source` message to write.
  /// @param recursion_level.
  /// @return WriteResult.
  [[nodiscard]] auto write_blocking(const span<uint8_t>& source, uint8_t recursion_level) noexcept -> WriteResult;

  /// @brief does not block while waiting for readers to catch up, returns `WriteResult::Repeat` instead.
  /// @param `source` message to write.
  /// @return `WriteResult`.
  [[nodiscard]] auto write_non_blocking(const span<uint8_t>& source) noexcept -> WriteResult;

  /// @brief Blocks/busy-spins while waiting for readers to catch up during a wraparound.
  /// @param recursion_level.
  /// @return std::optional<A*> -- pointer to allocated message or `null_opt` if error happened.
  template <class A>
    requires(std::default_initializable<A> && sizeof(A) != 0 && sizeof(A) <= M)
  [[nodiscard]] inline auto allocate_blocking(uint8_t recursion_level) noexcept -> std::optional<A*>;

  /// @brief does not block while waiting for readers to catch up, returns `std::null_opt` instead.
  /// @return `std::optional<A*>` -- pointer to allocated message or `null_opt` if there is no space left in the buffer.
  template <class A>
    requires(std::default_initializable<A> && sizeof(A) != 0 && sizeof(A) <= M)
  [[nodiscard]] inline auto allocate_non_blocking() noexcept -> std::optional<A*>;

  auto wait_for_readers_to_catch_up_and_wraparound() noexcept -> void;

  inline auto initiate_wraparound() noexcept -> void;

  inline auto complete_wraparound() noexcept -> void;

  [[nodiscard]] auto all_readers_caught_up() const noexcept -> bool;

  auto increment_message_count() noexcept -> void {
    this->message_count_ += 1;
    this->downstream_sequence_->store(this->message_count_, std::memory_order_release);
  }
};

template <size_t L, uint16_t M, bool B>
auto DemuxWriter<L, M, B>::write_blocking(const span<uint8_t>& source, uint8_t recursion_level) noexcept
    -> WriteResult {
  // it either writes the entire message or nothing
  const size_t written = this->buffer_.write(this->position_, source);
  if (written > 0) {
    this->position_ += written;
    this->increment_message_count();
    return WriteResult::Success;
  } else {
    if (recursion_level > 1) {
      LOG_ERROR << "[DemuxWriter::write_blocking] recursion_level: " << recursion_level;
      return WriteResult::Error;
    } else {
      this->wait_for_readers_to_catch_up_and_wraparound();
      return this->write_blocking(source, recursion_level + 1);
    }
  }
}

template <size_t L, uint16_t M, bool B>
auto DemuxWriter<L, M, B>::write_non_blocking(const span<uint8_t>& source) noexcept -> WriteResult {
  const size_t n = source.size();

  if (n == 0 || n > M) {
    LOG_ERROR << "[DemuxWriter::write_non_blocking] invalid message length: " << n;
    return WriteResult::Error;
  }

  if (this->wraparound_) {
    if (this->all_readers_caught_up()) {
      this->complete_wraparound();
    } else {
      return WriteResult::Repeat;
    }
  }

  // writes the entire message or nothing
  const size_t written = this->buffer_.write(this->position_, source);
  if (written > 0) {
    this->position_ += written;
    this->increment_message_count();
    return WriteResult::Success;
  } else {
    this->initiate_wraparound();
    return WriteResult::Repeat;
  }
}
template <size_t L, uint16_t M, bool B>
template <class A>
  requires(std::default_initializable<A> && sizeof(A) != 0 && sizeof(A) <= M)
[[nodiscard]] inline auto DemuxWriter<L, M, B>::allocate_blocking(uint8_t recursion_level) noexcept
    -> std::optional<A*> {
  std::optional<A*> result = this->buffer_.template allocate<A>(this->position_);
  if (result.has_value()) {
    return result;
  } else {
    if (recursion_level > 1) {
      LOG_ERROR << "[DemuxWriter::allocate_blocking] recursion_level: " << recursion_level;
      return std::nullopt;
    } else {
      this->wait_for_readers_to_catch_up_and_wraparound();
      return this->allocate_blocking<A>(recursion_level + 1);
    }
  }
}

template <size_t L, uint16_t M, bool B>
template <class A>
  requires(std::default_initializable<A> && sizeof(A) != 0 && sizeof(A) <= M)
[[nodiscard]] inline auto DemuxWriter<L, M, B>::allocate_non_blocking() noexcept -> std::optional<A*> {
  if (this->wraparound_) {
    if (this->all_readers_caught_up()) {
      this->complete_wraparound();
    } else {
      return std::nullopt;
    }
  }

  auto result = this->buffer_.template allocate<A>(this->position_);
  if (!result.has_value()) {
    this->initiate_wraparound();
  }
  return result;
}

template <size_t L, uint16_t M, bool B>
auto DemuxWriter<L, M, B>::wait_for_readers_to_catch_up_and_wraparound() noexcept -> void {
  this->initiate_wraparound();

  LOG_DEBUG << "[DemuxWriter::wait_for_readers_to_catch_up_and_wraparound] message_count_: " << this->message_count_
            << ", position_: " << this->position_ << " ... waiting ...";

  // busy-wait
  while (!this->all_readers_caught_up()) {
    _mm_pause();
  }

  this->complete_wraparound();
}

template <size_t L, uint16_t M, bool B>
inline auto DemuxWriter<L, M, B>::initiate_wraparound() noexcept -> void {
  // to mark wrap-around: write empty message if there is enough space (2); increment sequence number.
  this->wraparound_ = true;
  std::ignore = this->buffer_.write(this->position_, {});
  this->increment_message_count();
}

template <size_t L, uint16_t M, bool B>
inline auto DemuxWriter<L, M, B>::complete_wraparound() noexcept -> void {
  this->position_ = 0;
  this->wraparound_ = false;
}

template <size_t L, uint16_t M, bool B>
inline auto DemuxWriter<L, M, B>::all_readers_caught_up() const noexcept -> bool {
  return std::ranges::all_of(this->upstream_sequences_, [this](const auto* x) {
    return x->load(std::memory_order_acquire) == this->message_count_;
  });
}

}  // namespace lshl::demux::core