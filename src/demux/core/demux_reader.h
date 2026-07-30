// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <utility>
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

using lshl::demux::util::is_power_of_2;

/// @brief Demultiplexer reader. Should be mapped into shared memory allocated by DemuxWriter.
/// @tparam `L` The size of the circular buffer in bytes.
/// @tparam `M` The max message size in bytes.
template <size_t L, uint16_t M>
class DemuxReader {
  static_assert(L >= M + 2, "Buffer size L must be at least M + 2");
  static_assert(M > 0, "M must be greater than 0");
  static_assert(is_power_of_2(L), "Buffer size L must be a power of 2");

 private:
  // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
  const ReaderId id_;

  size_t position_{0};
  uint64_t available_message_count_{0};
  uint64_t read_message_count_{0};

  const MessageBuffer<L> buffer_;  // read only buffer
  const atomic<uint64_t>* downstream_sequence_;
  atomic<uint64_t>* upstream_sequence_;
  // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

 public:
  DemuxReader(
      const ReaderId& reader_id,
      const span<uint8_t, L> buffer,
      const atomic<uint64_t>* downstream_sequence,
      atomic<uint64_t>* upstream_sequence
  ) noexcept
      : id_(reader_id),
        buffer_(buffer),
        downstream_sequence_(downstream_sequence),
        upstream_sequence_(upstream_sequence) {
    LOG_INFO << "[DemuxReader::constructor] " << *this;
  }

  ~DemuxReader() = default;

  DemuxReader(const DemuxReader&) = delete;                     // no copy constructor, no reason to copy it
  auto operator=(const DemuxReader&) -> DemuxReader& = delete;  // no copy assignment
                                                                //
  DemuxReader(DemuxReader&&) = default;                         // movable, can be used with vector.emplace_back
  auto operator=(DemuxReader&&) -> DemuxReader& = delete;       // no move assignment

  template <size_t L1, uint16_t M1>
  friend auto operator<<(std::ostream& os, const DemuxReader<L1, M1>& reader) -> std::ostream&;

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
      assert(raw.size() == sizeof(T));
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      const T* x = reinterpret_cast<const T*>(raw.data());
      return std::make_optional(std::move(x));
    }
  }

  [[nodiscard]] auto is_id(const ReaderId& id) const noexcept -> bool { return this->id_ == id; }

  [[nodiscard]] auto id() const noexcept -> const ReaderId& { return this->id_; }

  [[nodiscard]] auto has_next() noexcept -> bool;

  [[nodiscard]] auto message_count() const noexcept -> uint64_t { return this->read_message_count_; }
};

template <size_t L, uint16_t M>
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
  } else {
    LOG_DEBUG << "[DemuxReader::next()] wrapping up, " << this->id_
              << ", read_message_count_: " << this->read_message_count_
              << ", available_message_count_: " << this->available_message_count_ << ", position_: " << this->position_;
    // signal received that we reached the end of buffer and need to wrap around
    assert(this->read_message_count_ == this->available_message_count_);
    this->position_ = 0;
  }

  this->upstream_sequence_->store(read_message_count_, std::memory_order_release);
  return result;
}

template <size_t L, uint16_t M>
[[nodiscard]] auto DemuxReader<L, M>::has_next() noexcept -> bool {
  // TODO(Leonid): maybe you should not use the cached values, keep reading from downstream_sequence_ to keep it hot
  //  Gemini says keep using cached value instead of polling the std:atomic, cache eviction is unlikely -- PROVE THIS!
  if (this->read_message_count_ < this->available_message_count_) {
    return true;
  } else {
    const uint64_t x = this->downstream_sequence_->load(std::memory_order_acquire);
    if (x > this->available_message_count_) {
      this->available_message_count_ = x;
      return true;
    } else {
      return false;
    }
  }
}

template <size_t L1, uint16_t M1>
auto operator<<(std::ostream& os, const DemuxReader<L1, M1>& reader) -> std::ostream& {
  os << "DemuxReader{L: " << L1 << ", M: " << M1 << ", id: " << reader.id() << ", position: " << reader.position_
     << ", available_message_count: " << reader.available_message_count_
     << ", read_message_count: " << reader.read_message_count_
     << ", downstream_sequence: " << reader.downstream_sequence_->load(std::memory_order_relaxed)
     << ", upstream_sequence: " << reader.upstream_sequence_->load(std::memory_order_relaxed);
  return os;
}
}  // namespace lshl::demux::core