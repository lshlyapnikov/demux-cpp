// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <vector>
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
using std::vector;

/// There is no specific reason for this limit, other then number of CPU cores available on typical systems.
constexpr size_t MAX_READER_NUM = 64;

/**
 * @brief Demultiplexer writer.
 *
 * @tparam M The type of the messages.
 * @tparam N The number of messages/positions in the circular buffer.
 * @tparam B Blocking/Non-blocking flag. If `true`, `emplace` or `write` operation will block/busy-spin
 *    while waiting for free space. If `false`, it will return immediately with empty optional.
 */
template <typename M, size_t N, bool B>
class DemuxWriter {
  /// Special value indicating that tail is not set.
  static constexpr size_t UNSET_TAIL = std::numeric_limits<size_t>::max();

  static_assert((N & (N - 1)) == 0, "N must be a power of 2 for optimization");
  static_assert(N > 0, "N must be greater than 0");
  static_assert(N < UNSET_TAIL, "numeric_limits<size_t>::max() used for UNSET_TAIL");

  static_assert(std::is_default_constructible_v<M>, "Must have default ctor");
  static_assert(std::is_move_constructible_v<M>, "Must have move ctor");
  static_assert(std::is_move_assignable_v<M>, "Must have move assign");

 private:
  /// @brief Circular buffer for writing messages.
  array<M, N>* buffer_;

  /// @brief Tail is the Writer's position, the position of the last written message.
  atomic<size_t>* tail_;

  /// @brief Reader heads, the positions of the last read messages for each reader. Const because Writer does not modify
  /// them.
  vector<const atomic<size_t>*> heads_;

  /// @brief Reader active heads, indicates whether each reader is active. Const because Writer does not modify them.
  vector<bool> active_readers_;

  size_t next_tail_{UNSET_TAIL};

 public:
  /**
   * @brief Constructs a DemuxWriter seq seq for allocating or deallocating the provided pointers.
   *  Write writes into the tail position, readers read from their respective head positions.
   *
   * @param buffer The shared circular buffer for message storage.
   * @param tail A pointer to the atomic head, which points to the last written message.
   * @param heads An array of pointers to atomic reader heads, each pointing to the last read message for each
   * reader.
   */
  DemuxWriter(
      array<M, N>* buffer,
      atomic<size_t>* tail,
      vector<const atomic<size_t>*> heads,
      vector<bool> active_readers_
  )
      : buffer_(buffer), tail_(tail), heads_(std::move(heads)), active_readers_(std::move(active_readers_)) {
    if (this->heads_.size() == 0) {
      throw std::invalid_argument("At least one reader required");
    }
    if (this->heads_.size() > MAX_READER_NUM) {
      throw std::invalid_argument("Too many readers, maximum is " + std::to_string(MAX_READER_NUM));
    }
    if (this->heads_.size() != this->active_readers_.size()) {
      throw std::invalid_argument("Heads and active readers size mismatch");
    }
    LOG_INFO << "[DemuxWriter::constructor] M: " << typeid(M).name() << ", N: " << N << ", B: " << B
             << ", state: " << *this;
  }

  ~DemuxWriter() = default;

  DemuxWriter(const DemuxWriter&) = delete;
  auto operator=(const DemuxWriter&) -> DemuxWriter& = delete;

  DemuxWriter(DemuxWriter&&) = default;
  auto operator=(DemuxWriter&&) -> DemuxWriter& = delete;

  [[nodiscard]] auto tail() const noexcept -> size_t { return this->tail_->load(std::memory_order_relaxed); }

  [[nodiscard]] auto next() noexcept -> std::optional<M*>;

  template <class... Args>
  [[nodiscard]] auto emplace(Args&&... args) noexcept -> bool;

  [[nodiscard]] auto commit() noexcept -> bool;

  template <typename M0, size_t N0, bool B0>
  friend auto operator<<(std::ostream& os, const DemuxWriter<M0, N0, B0>& writer) -> std::ostream&;

 private:
  [[nodiscard]] auto next_() noexcept -> std::optional<M*>;
};

template <typename M, size_t N, bool B>
auto DemuxWriter<M, N, B>::next() noexcept -> std::optional<M*> {
  std::optional<M*> ptr = this->next_();

  // If blocking mode is enabled, keep trying until we get a valid pointer
  if constexpr (B) {
    while (!ptr) {
      ptr = this->next_();
    }
  }

  return ptr;
}

template <typename M, size_t N, bool B>
template <class... Args>
auto DemuxWriter<M, N, B>::emplace(Args&&... args) noexcept -> bool {
  std::optional<M*> ptr = this->next();
  if (ptr) {
    std::destroy_at(*ptr);
    std::construct_at(*ptr, std::forward<Args>(args)...);
    return true;
  } else {
    return false;
  }
}

template <typename M, size_t N, bool B>
auto DemuxWriter<M, N, B>::next_() noexcept -> std::optional<M*> {
  const size_t current_tail = this->tail_->load(std::memory_order_relaxed);
  const size_t next_tail = (current_tail + 1) & (N - 1);  // wrap-around optimization for modulo N

  LOG_DEBUG << "[DemuxWriter::next] current_tail: " << current_tail << ", next_tail: " << next_tail;

  if (UNSET_TAIL != this->next_tail_) {
    LOG_WARNING << "[DemuxWriter::next] there is uncommited write at tail: " << current_tail;
    return std::nullopt;
  }

  // TODO(Leonid): find a way to avoid checking all readers every time, cache the slowest reader position?
  // Check every active reader's head
  for (size_t i = 0; i < active_readers_.size(); ++i) {
    if (active_readers_[i]) {
      // If the next tail catches up to ANY head, the buffer is full for that reader
      if (next_tail == heads_[i]->load(std::memory_order_acquire)) {
        return std::nullopt;
      }
    }
  }

  this->next_tail_ = next_tail;

  return &((*buffer_)[current_tail]);
}

template <typename M, size_t N, bool B>
auto DemuxWriter<M, N, B>::commit() noexcept -> bool {
  if (this->next_tail_ == UNSET_TAIL) {
    LOG_WARNING << "[DemuxWriter::commit] there is nothing to commit";
    return false;
  } else {
    this->tail_->store(this->next_tail_, std::memory_order_release);
    this->next_tail_ = UNSET_TAIL;
    return true;
  }
}

template <typename M, size_t N, bool B>
auto operator<<(std::ostream& os, const DemuxWriter<M, N, B>& writer) -> std::ostream& {
  os << "DemuxWriter{N: " << N << ", tail:" << writer.tail_->load(std::memory_order_relaxed) << ", next_tail:";
  if (writer.next_tail_ == DemuxWriter<M, N, B>::UNSET_TAIL) {
    os << "UNSET";
  } else {
    os << writer.next_tail_;
  }
  os << ", heads:[";
  for (size_t i = 0; i < writer.heads_.size(); ++i) {
    if (i > 0) {
      os << ", ";
    }
    os << writer.heads_[i]->load(std::memory_order_relaxed) << (writer.active_readers_[i] ? ":Y" : ":N");
  }
  os << "]}";
  return os;
}

}  // namespace lshl::demux::core
