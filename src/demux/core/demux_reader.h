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

/// @brief Demultiplexer reader. Should be mapped into shared memory allocated by DemuxWriter.
/// @tparam `L` The size of the circular buffer in bytes.
/// @tparam `M` The max message size in bytes.
template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
class DemuxReader {
 public:
  DemuxReader(
      const ReaderId& reader_id,
      const span<uint8_t, L> buffer,
      const atomic<uint64_t>* message_count_sync,
      atomic<uint64_t>* wraparound_sync
  ) noexcept
      : id_(reader_id),
        mask_(reader_id.mask()),
        buffer_(buffer),
        message_count_sync_(message_count_sync),
        wraparound_sync_(wraparound_sync) {
    LOG_INFO << "[DemuxReader::constructor] L: " << L << ", M: " << M << ", " << this->id_;
  }

  ~DemuxReader() = default;
  DemuxReader(const DemuxReader&) = delete;                     // no copy constructor, no reason to copy it
  auto operator=(const DemuxReader&) -> DemuxReader& = delete;  // no copy assignment
  DemuxReader(DemuxReader&&) = default;                         // movable, can be used with vector.emplace_back
  auto operator=(DemuxReader&&) -> DemuxReader& = delete;       // no move assignment

  /// @brief Does not block. Calls `has_next`. Returns a `span` pointing to the object in the circular buffer.
  ///   Do not keep the reference to the returned `span` between `next` calls, the underlying bytes can be overriden
  ///   when circular buffer wraps around. Copy the content of the returned span if you need to keep the reference.
  /// @return message or empty span if no data available.
  [[nodiscard]] auto next() noexcept -> const span<uint8_t>;

  /// @brief Receives a reference to the next message in the circular buffer and deserializes the raw bytes into the
  /// object of type `T` using `reinterpret_cast`. `T` must be a simple, flat class without references to other
  /// objects; otherwise, behavior is unspecified. Consider using custom serialization/deserialization with `write`
  /// and `next` that take and return `span`.
  /// @see [reinterpret_cast documentation](https://en.cppreference.com/w/cpp/language/reinterpret_cast)
  /// @tparam T
  /// @return optional `T`.
  template <class T>
  [[nodiscard]] auto next_unsafe() noexcept -> std::optional<const T*> {
    const span<uint8_t> raw = this->next();
    if (raw.empty()) {
      return std::nullopt;
    } else {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      const T* x = reinterpret_cast<const T*>(raw.data());
      return std::make_optional(std::move(x));
    }
  }

  [[nodiscard]] auto is_id(const ReaderId& id) const noexcept -> bool { return this->mask_ == id.mask(); }

  [[nodiscard]] auto id() const noexcept -> const ReaderId& { return this->id_; }

  [[nodiscard]] auto has_next() noexcept -> bool;

  [[nodiscard]] auto message_count() const noexcept -> uint64_t { return this->read_message_count_; }

 private:
  // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
  const ReaderId id_;
  const uint64_t mask_;  // micro-optimization

  size_t position_{0};
  uint64_t available_message_count_{0};
  uint64_t read_message_count_{0};

  const MessageBuffer<L> buffer_;  // read only buffer
  const atomic<uint64_t>* message_count_sync_;
  atomic<uint64_t>* wraparound_sync_;
  // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
};

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
// NOLINTNEXTLINE(readability-const-return-type)
[[nodiscard]] auto DemuxReader<L, M>::next() noexcept -> const span<uint8_t> {
  LOG_DEBUG << "[DemuxReader::next()] " << this->id_ << ", read_message_count_: " << this->read_message_count_
            << ", position_: " << this->position_;
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
    LOG_DEBUG << "[DemuxReader::next()] continue, " << this->id_
              << ", read_message_count_: " << this->read_message_count_
              << ", available_message_count_: " << this->available_message_count_ << ", position_: " << this->position_;
    assert(this->position_ <= L);
    return result;
  } else {
    LOG_DEBUG << "[DemuxReader::next()] wrapping up, " << this->id_
              << ", read_message_count_: " << this->read_message_count_
              << ", available_message_count_: " << this->available_message_count_ << ", position_: " << this->position_;
    // signal that it is ready to wraparound, see doc/adr/ADR003.md for more details
    assert(this->read_message_count_ == this->available_message_count_);
    this->position_ = 0;
    this->wraparound_sync_->fetch_or(this->mask_);
    return {};
  }
}

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
[[nodiscard]] auto DemuxReader<L, M>::has_next() noexcept -> bool {
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

}  // namespace lshl::demux::core