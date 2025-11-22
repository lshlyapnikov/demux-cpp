// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
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

using std::atomic;
using std::size_t;
using std::span;
using std::uint64_t;
using std::uint8_t;
using std::vector;

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
 * @tparam L The size of the circular buffer in bytes. When allocating the buffer in shared memory,
 * ensure that `L` is a multiple of the OS page size. This is because the Linux operating system
 * maps memory in whole pages, preventing memory waste.
 * @tparam M The maximum message size in bytes.
 * @tparam B If `true`, `write` will block/busy-spin while waiting for all readers to catch up. If `false`, it will
 * return immediately with `WriteResult::Repeat`.
 */
template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
class DemuxWriter {
 private:
  /// @brief Circular buffer for writing messages.
  MessageBuffer<L> buffer_;
  /// @brief Writer sequence number shared with readers.
  atomic<uint64_t>* writer_sequence_;
  /// @brief Reader positions within the circular buffer.
  vector<atomic<size_t>*> reader_positions_;
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
      vector<atomic<size_t>*> reader_positions
  ) noexcept
      : buffer_(buffer), writer_sequence_(writer_sequence), reader_positions_(std::move(reader_positions)) {
    LOG_INFO << "[DemuxWriter::constructor] L: " << L << ", M: " << M << ", B: " << B << " " << *this;
    for (const auto& x : this->reader_positions_) {
      LOG_DEBUG << "\treader_position: " << x->load();
    }
  }

  ~DemuxWriter() = default;
  DemuxWriter(const DemuxWriter&) = delete;
  auto operator=(const DemuxWriter&) -> DemuxWriter& = delete;
  DemuxWriter(DemuxWriter&&) = default;
  auto operator=(DemuxWriter&&) -> DemuxWriter& = delete;

  template <size_t L0, uint16_t M0, bool B0>
  friend auto operator<<(std::ostream& os, const DemuxWriter<L0, M0, B0>& writer) -> std::ostream&;

  [[nodiscard]] auto sequence() const noexcept -> uint64_t { return this->writer_sequence_->load(); }

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

 private:
  /**
   * @brief writes a message, source checks are supposed to be done by the caller.
   * @param source message to write.
   * @return `WriteResult`.
   */
  [[nodiscard]] auto write_(const span<uint8_t>& source) noexcept -> WriteResult;

  auto ensure_buffer_space(const size_t required) noexcept -> WriteResult;

  auto commit_(const size_t written) -> void;

  auto calculate_remaining(const size_t required) noexcept -> void;

  /**
   * @brief initiate buffer wraparound, write a wraparound marker if there are 2 bytes available.
   * All safety checks has to be done by the caller of this method.
   */
  auto wraparound_(const size_t min_reader_position) noexcept -> void;
};

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
[[nodiscard]] auto DemuxWriter<L, M, B>::write(const span<uint8_t>& source) noexcept -> WriteResult {
  const size_t n = source.size();
  if (n == 0 || n > M) {
    LOG_ERROR << "[DemuxWriter::write] invalid message length: " << n << ", state: " << *this;
    return WriteResult::Error;
  }
  return this->write_(source);
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
template <uint16_t N>
  requires(0 < N && N <= M)
[[nodiscard]] auto DemuxWriter<L, M, B>::write_safe(const span<uint8_t, N>& source) noexcept -> WriteResult {
  this->write_(source);
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
auto DemuxWriter<L, M, B>::write_(const span<uint8_t>& source) noexcept -> WriteResult {
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

  this->commit_(written);

  return WriteResult::Success;
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
auto DemuxWriter<L, M, B>::ensure_buffer_space(const size_t required) noexcept -> WriteResult {
  if constexpr (B) {
    // busy-spin waiting for the remaining space to increase
    while (this->remaining_ < required) {
      calculate_remaining(required);
    }
    return WriteResult::Success;
  } else {
    // non-blocking implementation return WriteResult::Repeat when the write operation has to be repeated
    if (this->remaining_ < required) {
      calculate_remaining(required);
      if (this->remaining_ < required) {
        return WriteResult::Repeat;
      } else {
        return WriteResult::Success;
      }
    }
    return WriteResult::Success;
  }
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
auto DemuxWriter<L, M, B>::commit_(const size_t written) -> void {
  // move position
  this->position_ += written;

  // update remaining space
  this->remaining_ -= written;

  // increment sequence to let readers know there is another message to read
  this->writer_sequence_->fetch_add(1);

  LOG_DEBUG << "[DemuxWriter::commit_] state: " << *this;
  assert(this->position_ <= L);
  assert(this->remaining_ <= L);
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
auto DemuxWriter<L, M, B>::calculate_remaining(const size_t required) noexcept -> void {
  // this should always be true
  assert(this->position_ + this->remaining_ <= L);
  // should calculate_remaining only when not enough space for required
  assert(required > this->remaining_);

  size_t slowest_reader_position = L;
  size_t min_reader_position = L;

  for (const auto& reader_position : this->reader_positions_) {
    const uint64_t x = reader_position->load();
    if (x > this->position_ && x < slowest_reader_position) {
      slowest_reader_position = x;
    }
    min_reader_position = std::min(x, min_reader_position);
  }

  if (slowest_reader_position == L) {
    // all readers are able to keep up with the writer, the rest of the buffer is available for writing
    this->remaining_ = L - this->position_;
    if (this->remaining_ < required) {
      // if there isn't enough space, wrapround
      this->wraparound_(min_reader_position);
    }
  } else {
    // -1 is to avoid stepping on the slowest reader position
    this->remaining_ = slowest_reader_position - 1 - this->position_;
  }

  LOG_DEBUG << "[DemuxWriter::calculate_remaining] state: " << *this
            << ", slowest_reader_position: " << slowest_reader_position;
  assert(this->remaining_ <= L);
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
inline auto DemuxWriter<L, M, B>::wraparound_(const size_t min_reader_position) noexcept -> void {
  if (0 == min_reader_position) {
    // can't wraparound at this moment, wait for the reader to move, keep the current writer position
  } else {
    if (this->remaining_ >= sizeof(message_length_t)) {
      // write wraparound marker (empty message)
      const size_t written = this->buffer_.write(this->position_, {});
      assert(written == sizeof(message_length_t));
    }
    // reset writer position
    this->position_ = 0;
    // -1 is to avoid stepping on the slowest reader position
    this->remaining_ = min_reader_position - 1;
    // increment sequence to let readers know there is another message to read
    this->writer_sequence_->fetch_add(1);
  }

  LOG_DEBUG << "[DemuxWriter::wraparound_] state: " << *this << ", min_reader_position: " << min_reader_position;
  assert(this->remaining_ <= L);
}

template <size_t L, uint16_t M, bool B>
auto operator<<(std::ostream& os, const DemuxWriter<L, M, B>& writer) -> std::ostream& {
  os << "DemuxWriter{sequence: " << writer.sequence() << ", position: " << writer.position_
     << ", remaining: " << writer.remaining_ << "}";
  return os;
}

}  // namespace lshl::demux::core
