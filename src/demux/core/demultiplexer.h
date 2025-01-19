// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#ifndef DEMUX_CPP_LSHL_DEMUX_DEMULTIPLEXER_H
#define DEMUX_CPP_LSHL_DEMUX_DEMULTIPLEXER_H

#include <atomic>
#include <boost/log/trivial.hpp>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>
#include "./message_buffer.h"
#include "./reader_id.h"

namespace lshl::demux::core {

using std::atomic;
using std::size_t;
using std::span;
using std::uint64_t;
using std::uint8_t;

enum WriteResult : std::uint8_t {
  Success,  // message sent
  Repeat,   // wraparound required, resend the last message
  Error,    // message cannot be sent, log error and drop it
};

auto mask_to_reader_ids(const uint64_t value) -> std::vector<ReaderId>;

/// @brief Demultiplexer writer.
/// @tparam `L` The size of the circular buffer in bytes. When allocating the buffer in shared memory,
/// ensure that `L` is a multiple of the OS page size. This is because the Linux operating system
/// maps memory in whole pages, preventing memory waste.
/// @tparam `M` The maximum message size in bytes.
/// @tparam `B` If `true`, `write` will block/busy-spin while waiting for all readers to catch up during a
/// wraparound synchronization. If `false`, it will return immediately with `WriteResult::Repeat`.
template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
class DemuxWriter {
 public:
  DemuxWriter(
      uint64_t all_readers_mask,
      span<uint8_t, L> buffer,
      atomic<uint64_t>* message_count_sync,
      atomic<uint64_t>* wraparound_sync
  ) noexcept
      : all_readers_mask_(all_readers_mask),
        buffer_(buffer),
        message_count_sync_(message_count_sync),
        wraparound_sync_(wraparound_sync) {
    BOOST_LOG_TRIVIAL(info) << "[DemuxWriter::constructor] L: " << L << ", M: " << M << ", B: " << B
                            << ", all_readers_mask_: " << this->all_readers_mask_;
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
      BOOST_LOG_TRIVIAL(error) << "[DemuxWriter::write] invalid message length: " << n;
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
  /// `span`.
  /// @see [reinterpret_cast documentation](https://en.cppreference.com/w/cpp/language/reinterpret_cast)
  /// @tparam `T`
  /// @param `source` message that will be copied into the circular buffer.
  /// @return
  template <class T>
    requires(sizeof(T) <= M && sizeof(T) != 0)
  [[nodiscard]] auto write_object(const T& source) -> WriteResult {
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
      return {this->allocate_blocking<A>(1)};
    } else {
      return this->allocate_non_blocking<A>();
    }
  }

  template <class A>
    requires(sizeof(A) <= M && sizeof(A) != 0)
  auto commit() noexcept -> void {
    this->position_ += sizeof(A);
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

  [[nodiscard]] auto is_registered_reader(const ReaderId& id) const noexcept -> bool {
    return this->all_readers_mask_ & id.mask();
  }

  auto add_reader(const ReaderId& id) noexcept -> void { this->all_readers_mask_ |= id.mask(); }

  auto remove_reader(const ReaderId& id) noexcept -> void { this->all_readers_mask_ &= ~id.mask(); }

  /// @brief Returns lagging readers, based on `wraparound_sync_` and `all_readers_mask_`.
  /// iterates over 64bits.
  [[nodiscard]] auto lagging_readers() const noexcept -> std::vector<ReaderId>;

#ifdef UNIT_TEST

  auto data() const noexcept -> span<uint8_t> { return {this->buffers_.data(), this->position_}; }

  auto position() const noexcept -> size_t { return this->position_; }

  auto all_readers_mask() const noexcept -> uint64_t { return this->all_readers_mask_; }

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
  [[nodiscard]] auto allocate_blocking(uint8_t recursion_level) noexcept -> std::optional<A*>;

  /// @brief does not block while waiting for readers to catch up, returns `std::null_opt` instead.
  /// @return `std::optional<A*>` -- pointer to allocated message or `null_opt` if there is no space left in the buffer.
  template <class A>
    requires(std::default_initializable<A> && sizeof(A) != 0 && sizeof(A) <= M)
  [[nodiscard]] auto allocate_non_blocking() noexcept -> std::optional<A*> {
    return this->buffer_.template allocate<A>(this->position_);
  }

  auto wait_for_readers_to_catch_up_and_wraparound() noexcept -> void;

  inline auto initiate_wraparound() noexcept -> void;

  inline auto complete_wraparound() noexcept -> void;

  [[nodiscard]] auto all_readers_caught_up() noexcept -> bool;

  auto increment_message_count() noexcept -> void {
    this->message_count_ += 1;
    this->message_count_sync_->store(this->message_count_);
  }

  uint64_t all_readers_mask_;

  size_t position_{0};
  uint64_t message_count_{0};
  MessageBuffer<L> buffer_;
  bool wraparound_{false};
  atomic<uint64_t>* message_count_sync_;
  atomic<uint64_t>* wraparound_sync_;
};

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
    BOOST_LOG_TRIVIAL(info) << "[DemuxReader::constructor] L: " << L << ", M: " << M << ", " << this->id_;
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

  [[nodiscard]] auto is_id(const ReaderId& id) const noexcept -> bool { return this->mask_ == id.mask(); }

  [[nodiscard]] auto id() const noexcept -> const ReaderId& { return this->id_; }

  [[nodiscard]] auto has_next() noexcept -> bool;

  [[nodiscard]] auto message_count() const noexcept -> uint64_t { return this->read_message_count_; }

#ifdef UNIT_TEST
  auto data() const noexcept -> span<uint8_t> { return {this->buffers_.data(), this->position_}; }

  auto position() const noexcept -> size_t { return this->position_; }

  auto mask() const noexcept -> uint64_t { return this->all_readers_mask_; }
#endif  // UNIT_TEST

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

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
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
      BOOST_LOG_TRIVIAL(error) << "[DemuxWriter::write_blocking] recursion_level: " << recursion_level;
      return WriteResult::Error;
    } else {
      this->wait_for_readers_to_catch_up_and_wraparound();
      return this->write_blocking(source, recursion_level + 1);
    }
  }
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
auto DemuxWriter<L, M, B>::write_non_blocking(const span<uint8_t>& source) noexcept -> WriteResult {
  const size_t n = source.size();

  if (n == 0 || n > M) {
    BOOST_LOG_TRIVIAL(error) << "[DemuxWriter::write_non_blocking] invalid message length: " << n;
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
  requires(L >= M + 2 && M > 0)
template <class A>
  requires(std::default_initializable<A> && sizeof(A) != 0 && sizeof(A) <= M)
[[nodiscard]] auto DemuxWriter<L, M, B>::allocate_blocking(uint8_t recursion_level) noexcept -> std::optional<A*> {
  std::optional<A*> result = this->buffer_.template allocate<A>(this->position_);
  if (result.has_value()) {
    return result;
  } else {
    if (recursion_level > 1) {
      BOOST_LOG_TRIVIAL(error) << "[DemuxWriter::allocate_blocking] recursion_level: " << recursion_level;
      return std::nullopt;
    } else {
      this->wait_for_readers_to_catch_up_and_wraparound();
      return this->allocate_blocking(recursion_level + 1);
    }
  }
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
auto DemuxWriter<L, M, B>::wait_for_readers_to_catch_up_and_wraparound() noexcept -> void {
  this->initiate_wraparound();

#ifndef NDEBUG
  BOOST_LOG_TRIVIAL(debug) << "[DemuxWriter::wait_for_readers_to_catch_up_and_wraparound] message_count_: "
                           << this->message_count_ << ", position_: " << this->position_ << " ... waiting ...";
#endif

  // busy-wait
  while (!this->all_readers_caught_up()) {
  }

  this->complete_wraparound();
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
inline auto DemuxWriter<L, M, B>::initiate_wraparound() noexcept -> void {
  // see doc/adr/ADR003.md for more details
  this->wraparound_ = true;
  this->wraparound_sync_->store(0);
  std::ignore = this->buffer_.write(this->position_, {});
  this->increment_message_count();
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
inline auto DemuxWriter<L, M, B>::complete_wraparound() noexcept -> void {
  this->position_ = 0;
  this->wraparound_ = false;
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
inline auto DemuxWriter<L, M, B>::all_readers_caught_up() noexcept -> bool {
  const uint64_t x = this->wraparound_sync_->load();
  return x == this->all_readers_mask_;
}

template <size_t L, uint16_t M, bool B>
  requires(L >= M + 2 && M > 0)
[[nodiscard]] auto DemuxWriter<L, M, B>::lagging_readers() const noexcept -> std::vector<ReaderId> {
  const uint64_t reader_ids = this->wraparound_sync_->load();
  const uint64_t lagging_reader_ids = reader_ids ^ this->all_readers_mask_;
  return mask_to_reader_ids(lagging_reader_ids);
}

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
// NOLINTNEXTLINE(readability-const-return-type)
[[nodiscard]] auto DemuxReader<L, M>::next() noexcept -> const span<uint8_t> {
#ifndef NDEBUG
  BOOST_LOG_TRIVIAL(debug) << "[DemuxReader::next()] " << this->id_
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
    BOOST_LOG_TRIVIAL(debug) << "[DemuxReader::next()] continue, " << this->id_
                             << ", read_message_count_: " << this->read_message_count_
                             << ", available_message_count_: " << this->available_message_count_
                             << ", position_: " << this->position_;
#endif
    assert(this->position_ <= L);
    return result;
  } else {
#ifndef NDEBUG
    BOOST_LOG_TRIVIAL(debug) << "[DemuxReader::next()] wrapping up, " << this->id_
                             << ", read_message_count_: " << this->read_message_count_
                             << ", available_message_count_: " << this->available_message_count_
                             << ", position_: " << this->position_;
#endif
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

#endif  // DEMUX_CPP_LSHL_DEMUX_DEMULTIPLEXER_H