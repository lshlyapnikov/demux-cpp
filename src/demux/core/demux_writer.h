// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>
#include "../util/boost_log_util.h"
#include "./message_buffer.h"
#include "./reader_id.h"

namespace lshl::demux::core {

using std::array;
using std::atomic;
using std::size_t;
using std::span;
using std::uint64_t;
using std::uint8_t;

/**
 * @brief The result of a write operation to the DemuxWriter.
 */
enum WriteResult : std::uint8_t {
  /// The message was successfully written to the buffer.
  Success,

  /// The write operation could not be completed at this time, usually due to insufficient space in the buffer.
  /// The caller should repeat the attempt. This is used in non-blocking mode.
  Repeat,

  /// An unrecoverable error occurred, and the message could not be written. The message should be considered dropped.
  Error,
};

/**
 * @brief Demultiplexer writer.
 * @tparam R The max number of readers supported.
 * @tparam L The size of the circular buffer in bytes. When allocating the buffer in shared memory,
 * ensure that `L` is a multiple of the OS page size. This is because the Linux operating system
 * maps memory in whole pages, preventing memory waste.
 * @tparam M The maximum message size in bytes.
 * @tparam B If `true`, `write` will block/busy-spin while waiting for all readers to catch up. If `false`, it will
 * return immediately with `WriteResult::Repeat`.
 */
template <uint8_t R, size_t L, uint16_t M, bool B>
  requires(R > 0 && L >= M + 2 && M > 0)
class DemuxWriter {
 private:
  /// @brief Circular buffer for writing messages.
  MessageBuffer<L> buffer_;
  /// @brief Writer sequence number shared with readers.
  atomic<uint64_t>* writer_sequence_;
  /// @brief Reader positions within the circular buffer.
  array<atomic<size_t>*, R> reader_positions_{};
  /// @brief Writer position.
  size_t position_{0};
  /// @brief Free Space to write new messages without overwriting unread data and without a wraparound.
  size_t remaining_{L};

 public:
  /**
   * @brief Constructs a DemuxWriter that is not responsible for allocating or deallocating the provided pointers.
   * @param buffer The shared circular buffer for message storage.
   * @param writer_sequence A pointer to the atomic writer sequence counter, shared with all readers for
   * synchronization.
   * @param reader_positions An array of pointers to atomic reader position counters, one for each reader, to track
   * their progress in the buffer.
   */
  DemuxWriter(
      span<uint8_t, L> buffer,
      atomic<uint64_t>* writer_sequence,
      array<atomic<size_t>*, R> reader_positions
  ) noexcept
      : buffer_(buffer), writer_sequence_(writer_sequence), reader_positions_(reader_positions) {
    LOG_INFO << "[DemuxWriter::constructor] " << *this;
  }

  ~DemuxWriter() = default;
  DemuxWriter(const DemuxWriter&) = delete;
  auto operator=(const DemuxWriter&) -> DemuxWriter& = delete;
  DemuxWriter(DemuxWriter&&) = default;
  auto operator=(DemuxWriter&&) -> DemuxWriter& = delete;

  auto sequence() noexcept -> uint64_t { return this->writer_sequence_->load(); }

  /**
   * @brief Writes/copies the `source` into the buffer.
   * @param source message that will be copied into the circular buffer.
   * @return WriteResult
   */
  [[nodiscard]] auto write(const span<uint8_t>& source) noexcept -> WriteResult;

  /**
   * @brief The `span` size is checked at compile time, see the `requires` clause.
   * @tparam N the message size.
   * @param source message that will be copied into the circular buffer.
   * @return WriteResult
   */
  template <uint16_t N>
    requires(0 < N && N <= M)
  [[nodiscard]] auto write_safe(const span<uint8_t, N>& source) noexcept -> WriteResult;

  /**
   * @brief allocates an object in the buffer.
   * @return `std::optional<A*>` -- pointer to allocated message or `null_opt` if there is no space left in the
   * buffer. The caller should repeat the attempt. This is used in non-blocking mode.
   */
  template <class A>
    requires(std::default_initializable<A> && sizeof(A) != 0 && sizeof(A) <= M)
  [[nodiscard]] auto allocate() noexcept -> std::optional<A*>;

  template <class A>
    requires(sizeof(A) <= M && sizeof(A) != 0)
  auto commit() noexcept -> void {
    commit(MessageBuffer<0>::required<A>());
  }

  template <uint8_t R_T, size_t L_T, uint16_t M_T, bool B_T>
  friend auto operator<<(std::ostream& os, const DemuxWriter<R_T, L_T, M_T, B_T>& writer) -> std::ostream&;

 private:
  /**
   * @brief writes a message, source checks are supposed to be done by the caller.
   * @param source message to write.
   * @return `WriteResult`.
   */
  [[nodiscard]] auto write_(const span<uint8_t>& source) noexcept -> WriteResult;

  auto ensure_buffer_space(const size_t required) noexcept -> WriteResult;

  auto commit(const size_t written) -> void;

  auto calculate_remaining(const size_t required) noexcept -> void;

  auto wraparound() noexcept -> void;
};

template <uint8_t R, size_t L, uint16_t M, bool B>
  requires(R > 0 && L >= M + 2 && M > 0)
[[nodiscard]] auto DemuxWriter<R, L, M, B>::write(const span<uint8_t>& source) noexcept -> WriteResult {
  const size_t n = source.size();
  if (n == 0 || n > M) {
    LOG_ERROR << "[DemuxWriter::write] invalid message length: " << n << ", state: " << *this;
    return WriteResult::Error;
  }
  return this->write_(source);
}

template <uint8_t R, size_t L, uint16_t M, bool B>
  requires(R > 0 && L >= M + 2 && M > 0)
template <uint16_t N>
  requires(0 < N && N <= M)
[[nodiscard]] auto DemuxWriter<R, L, M, B>::write_safe(const span<uint8_t, N>& source) noexcept -> WriteResult {
  this->write_(source);
}

template <uint8_t R, size_t L, uint16_t M, bool B>
  requires(R > 0 && L >= M + 2 && M > 0)
template <class A>
  requires(std::default_initializable<A> && sizeof(A) != 0 && sizeof(A) <= M)
[[nodiscard]] inline auto DemuxWriter<R, L, M, B>::allocate() noexcept -> std::optional<A*> {
  constexpr size_t n = sizeof(A);
  static_assert(n > 0 && n <= M);
  constexpr size_t required_space = sizeof(message_length_t) + n;

  if (this->ensure_buffer_space(required_space) == WriteResult::Repeat) {
    return WriteResult::Repeat;
  }

  std::optional<A*> result = this->buffer_.template allocate<A>(this->position_);
  if (result.has_value()) {
    // don't move the position and don't increment the sequence yet, commit() handles this;
    // the caller that allocated the object needs to populate it first and then call commit()
    return result;
  } else {
    LOG_ERROR << "[DemuxWriter::allocate] failed to allocate " << n << " bytes" << ", state: " << *this;
    return std::nullopt;
  }
}

template <uint8_t R, size_t L, uint16_t M, bool B>
  requires(R > 0 && L >= M + 2 && M > 0)
auto DemuxWriter<R, L, M, B>::write_(const span<uint8_t>& source) noexcept -> WriteResult {
  const size_t n = source.size();
  assert(n > 0 && n <= M);
  const size_t required = sizeof(message_length_t) + n;

  if (this->ensure_buffer_space(required) == WriteResult::Repeat) {
    return WriteResult::Repeat;
  }

  const size_t written = this->buffer_.write(this->position_, source);
  if (written != required) {
    LOG_ERROR << "[DemuxWriter::write_] written: " << written << ", expected: " << required << ", state: " << *this;
    return WriteResult::Error;
  }

  this->commit(written);

  return WriteResult::Success;
}

template <uint8_t R, size_t L, uint16_t M, bool B>
  requires(R > 0 && L >= M + 2 && M > 0)
auto DemuxWriter<R, L, M, B>::ensure_buffer_space(const size_t required) noexcept -> WriteResult {
  if constexpr (B) {
    // busy-spin waiting for the remaining space to increase
    while (this->remaining_ < required) {
      calculate_remaining();
    }
    return WriteResult::Success;
  } else {
    // non-blocking implementation return WriteResult::Repeat when the write operation has to be repeated
    if (this->remaining_ < required) {
      calculate_remaining();
      if (this->remaining_ < required) {
        return WriteResult::Repeat;
      } else {
        return WriteResult::Success;
      }
    }
  }
}

template <uint8_t R, size_t L, uint16_t M, bool B>
  requires(R > 0 && L >= M + 2 && M > 0)
auto DemuxWriter<R, L, M, B>::commit(const size_t written) -> void {
  // move position
  this->position_ += written;
  assert(this->position_ <= L);

  // update remaining space
  this->remaining_ -= written;
  assert(this->remaining_ <= L);

  // increment sequence to let readers know there is another message to read
  this->writer_sequence_->fetch_add(1);
}

template <uint8_t R, size_t L, uint16_t M, bool B>
  requires(R > 0 && L >= M + 2 && M > 0)
auto DemuxWriter<R, L, M, B>::calculate_remaining(const size_t required) noexcept -> void {
  // check if wraparound is required
  if (L - this->position_ < required) {
    this->wraparound();
    return;
  }

  size_t slowest_reader_position = L;

  for (const auto& reader_position : this->reader_positions_) {
    const uint64_t x = reader_position->load();
    if (x > this->position_ && x < slowest_reader_position) {
      slowest_reader_position = x;
    }
  }

  if (slowest_reader_position == L) {
    // all readers are able to keep up with the writer, the rest of the buffer is available for writing
    this->remaining_ = L - this->position_;
  } else {
    // -1 is to avoid stepping on the slowest reader position
    this->remaining_ = slowest_reader_position - 1 - this->position_;
  }

  LOG_DEBUG << "[DemuxWriter::calculate_remaining] slowest_reader_position: " << slowest_reader_position
            << ", state: " << *this;
  assert(this->remaining_ <= L);
}

template <uint8_t R, size_t L, uint16_t M, bool B>
  requires(R > 0 && L >= M + 2 && M > 0)
inline auto DemuxWriter<R, L, M, B>::wraparound() noexcept -> void {
  size_t slowest_reader_position = L;

  for (const auto& reader_position : this->reader_positions_) {
    const uint64_t x = reader_position->load();
    slowest_reader_position = std::min(x, slowest_reader_position);
  }

  if (0 == slowest_reader_position) {
    // can't wraparound at this moment, keep the current position
  } else {
    this->position_ = 0;
    // -1 is to avoid stepping on the slowest reader position
    this->remaining_ = slowest_reader_position - 1;
  }

  LOG_DEBUG << "[DemuxWriter::wrapaorund] slowest_reader_position: " << slowest_reader_position << ", state: " << *this;
  assert(this->remaining_ <= L);
}

template <uint8_t R, size_t L, uint16_t M, bool B>
auto operator<<(std::ostream& os, const DemuxWriter<R, L, M, B>& writer) -> std::ostream& {
  os << "DemuxWriter{R: " << static_cast<int>(R) << ", L: " << L << ", M: " << M << ", B: " << B
     << ", sequence: " << writer.sequence() << ", position: " << writer.position_
     << ", remaining: " << writer.remaining_;
  return os;
}

}  // namespace lshl::demux::core
