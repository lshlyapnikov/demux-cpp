// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

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
#include "./message_buffer.h"
#include "./reader_id.h"

namespace lshl::demux::core {

using std::atomic;
using std::size_t;
using std::span;
using std::uint64_t;
using std::uint8_t;

/**
 * @brief Demultiplexer reader. Should be mapped into shared memory allocated by DemuxWriter.
 * @tparam `L` The size of the circular buffer in bytes.
 * @tparam `M` The max message size in bytes.
 */
template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
class DemuxReader {
 private:
  // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
  const ReaderId id_;

  size_t position_{0};
  uint64_t last_writer_sequence_{0};
  uint64_t reader_sequence_{0};

  const MessageBuffer<L> buffer_;  // read only buffer
  const atomic<uint64_t>* writer_sequence_;
  atomic<size_t>* reader_position_;
  // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

 public:
  DemuxReader(
      const ReaderId& reader_id,
      const span<uint8_t, L> buffer,
      const atomic<uint64_t>* writer_sequence,
      atomic<size_t>* reader_position
  ) noexcept
      : id_(reader_id), buffer_(buffer), writer_sequence_(writer_sequence), reader_position_(reader_position) {
    LOG_INFO << "[DemuxReader::constructor] L: " << L << ", M: " << M << " " << *this;
  }

  ~DemuxReader() = default;
  DemuxReader(const DemuxReader&) = delete;                     // no copy constructor, no reason to copy it
  auto operator=(const DemuxReader&) -> DemuxReader& = delete;  // no copy assignment
  DemuxReader(DemuxReader&&) = default;                         // movable, can be used with vector.emplace_back
  auto operator=(DemuxReader&&) -> DemuxReader& = delete;       // no move assignment

  /**
   * @brief Does not block. Calls `has_next`. Returns a `span` pointing to the object in the circular buffer.
   *   Do not keep the reference to the returned `span` between `next` calls, the underlying bytes can be overriden
   *   when circular buffer wraps around. Copy the content of the returned span if you need to keep the reference.
   * @return message or empty span if no data available.
   */
  [[nodiscard]] auto next() noexcept -> span<const uint8_t>;

  /**
   * @brief Receives a reference to the next message in the circular buffer and deserializes the raw bytes into the
   * object of type `T` using `reinterpret_cast`. `T` must be a simple, flat class without references to other
   * objects; otherwise, behavior is unspecified. Consider using custom serialization/deserialization with `write`
   * and `next` that take and return `span`.
   * @see reinterpret_cast documentation
   * @tparam T
   * @return optional `T`.
   */
  template <class T>
  [[nodiscard]] auto next_unsafe() noexcept -> std::optional<const T*>;

  [[nodiscard]] auto id() const noexcept -> const ReaderId& { return this->id_; }

  [[nodiscard]] auto has_next() noexcept -> bool;

  [[nodiscard]] auto sequence() const noexcept -> uint64_t { return this->reader_sequence_; }

  [[nodiscard]] auto position() const noexcept -> size_t { return this->position_; }

  template <size_t L1, uint16_t M1>
  friend auto operator<<(std::ostream& os, const DemuxReader<L1, M1>& reader) -> std::ostream&;
};

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
[[nodiscard]] auto DemuxReader<L, M>::next() noexcept -> span<const uint8_t> {
  LOG_DEBUG << "[DemuxReader::next()] state0: " << *this;

  if (!this->has_next()) {
    return {};
  }

  const span<uint8_t>& result = this->buffer_.read(this->position_);
  this->reader_sequence_ += 1;
  const size_t result_size = result.size();
  assert(result_size <= M);

  if (result_size > 0) {
    this->position_ += (sizeof(message_length_t) + result_size);
    this->reader_position_->store(this->position_);
    LOG_DEBUG << "[DemuxReader::next()] state1: " << *this << ", result_size: " << result_size;
    assert(this->position_ <= L);
    return result;
  } else {
    // wraparound
    this->position_ = 0;
    this->reader_position_->store(0);
    LOG_DEBUG << "[DemuxReader::next()] state1: " << *this << ", result_size: " << result_size;
    return {};
  }
}

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
template <class T>
[[nodiscard]] auto DemuxReader<L, M>::next_unsafe() noexcept -> std::optional<const T*> {
  const span<uint8_t> raw = this->next();
  if (raw.empty()) {
    return std::nullopt;
  } else {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const T* x = reinterpret_cast<const T*>(raw.data());
    return std::make_optional(std::move(x));
  }
}

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
[[nodiscard]] auto DemuxReader<L, M>::has_next() noexcept -> bool {
  if (this->reader_sequence_ < this->last_writer_sequence_) {
    return true;
  } else {
    const uint64_t x = this->writer_sequence_->load();
    if (x > this->last_writer_sequence_) {
      this->last_writer_sequence_ = x;
      return true;
    } else {
      return false;
    }
  }
}

template <size_t L, uint16_t M>
auto operator<<(std::ostream& os, const DemuxReader<L, M>& reader) -> std::ostream& {
  os << "DemuxReader{id: " << reader.id() << ", last_writer_sequence_: " << reader.last_writer_sequence_
     << ", reader_sequence: " << reader.reader_sequence_ << ", position: " << reader.position_;
  return os;
}

}  // namespace lshl::demux::core