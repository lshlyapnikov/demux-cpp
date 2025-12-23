// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <ostream>
#include <span>
#include "../util/boost_log_util.h"
#include "./message_buffer.h"
#include "./reader_id.h"

namespace lshl::demux::core {

using std::array;
using std::atomic;
using std::optional;
using std::size_t;
using std::span;
using std::uint64_t;
using std::uint8_t;

/**
 * @brief Demultiplexer reader. Should be mapped into shared memory allocated by DemuxWriter.
 *
 * @tparam M The type of the messages.
 * @tparam N The number of messages/positions in the circular buffer.
 */
template <typename M, size_t N, bool B>

class DemuxReader {
 private:
  ReaderId id_;

  /// @brief Circular buffer for reading messages.
  array<M, N>* buffer_;

  /// @brief Tail is the Writer's position, the position of the last written message. Const because Reader does not
  /// write.
  const atomic<size_t>* tail_;

  /// @brief Reader's head, the positions of the last read message.
  atomic<size_t>* head_;

  uint64_t message_count_{0};

 public:
  DemuxReader(const ReaderId& reader_id, array<M, N>* buffer, const atomic<size_t>* tail, atomic<size_t>* head) noexcept
      : id_(reader_id), buffer_(buffer), tail_(tail), head_(head) {
    LOG_INFO << "[DemuxReader::constructor] M: " << typeid(M).name() << ", N: " << N << ", B: " << B
             << ", state: " << *this;
  }

  ~DemuxReader() = default;

  DemuxReader(const DemuxReader&) = delete;
  auto operator=(const DemuxReader&) -> DemuxReader& = delete;

  DemuxReader(DemuxReader&&) = default;
  auto operator=(DemuxReader&&) -> DemuxReader& = delete;

  [[nodiscard]] auto id() const noexcept -> const ReaderId& { return id_; }

  [[nodiscard]] auto tail() const noexcept -> size_t { return this->tail_->load(std::memory_order_relaxed); }

  [[nodiscard]] auto head() const noexcept -> size_t { return this->head_->load(std::memory_order_relaxed); }

  [[nodiscard]] auto message_count() const noexcept -> uint64_t { return this->message_count_; }

  /**
   * @brief Returns an `optional` pointing to the object in the circular buffer.
   *   Do not keep the reference to the returned `optional` between `next` calls, the underlying object can and will be
   *    overriden when circular buffer wraps around. Copy the content of the returned `optional` if you need to keep a
   *    reference.
   * @return `optional` message.
   */
  [[nodiscard]] auto next() noexcept -> optional<const M*>;

  template <typename M0, size_t N0, bool B0>
  friend auto operator<<(std::ostream& os, const DemuxReader<M0, N0, B0>& reader) -> std::ostream&;
};

template <typename M, size_t N, bool B>
auto DemuxReader<M, N, B>::next() noexcept -> optional<const M*> {
  size_t head = this->head_->load(std::memory_order_relaxed);
  size_t tail = this->tail_->load(std::memory_order_acquire);

  if (head == tail) {
    if constexpr (B) {
      while (head == tail) {
        tail = this->tail_->load(std::memory_order_acquire);
      }
    } else {
      return std::nullopt;
    }
  }

  const M* ptr = &(*buffer_)[head];
  const size_t next_head = (head + 1) & (N - 1);
  this->head_->store(next_head, std::memory_order_release);
  this->message_count_ += 1;

  return ptr;
}

template <typename M, size_t N, bool B>
auto operator<<(std::ostream& os, const DemuxReader<M, N, B>& reader) -> std::ostream& {
  os << "DemuxReader{id:" << reader.id_ << ", head:" << reader.head_->load(std::memory_order_relaxed)
     << ", tail:" << reader.tail_->load(std::memory_order_relaxed) << "}";
  return os;
}

}  // namespace lshl::demux::core