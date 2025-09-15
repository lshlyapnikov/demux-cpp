// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>

namespace lshl::demux::core {

using std::int64_t;
using std::size_t;
using std::span;
using std::uint16_t;
using std::uint8_t;

using message_length_t = uint16_t;

/// @brief MessageBuffer wraps a byte array, which can be allocated in shared memory. MessageBuffer does not take the
/// ownership of the passed buffer. The outside logic is responsible to freeing the passed buffer. External
/// synchronization is required for accessing the `data_` via `read` and `write` calls. Read or write `position` must be
/// stored outside of this class.
/// @tparam L total buffer size in bytes.
template <size_t L>
struct MessageBuffer {
 public:
  explicit MessageBuffer(span<uint8_t, L> buffer) : data_(buffer.data()) {}

  /// @brief Writes the entire passed message into the buffer or nothing.
  /// @param position -- the zero-based byte offset at which the message should be written.
  /// @param message -- the bytes that should be written.
  /// @return the total number of written bytes (2 + message length) or zero.
  [[nodiscard]] auto write(const size_t position, const span<uint8_t>& message) noexcept -> size_t {
    const size_t required_space = sizeof(message_length_t) + message.size();

    if (this->remaining(position) >= required_space) {
      const size_t length = message.size();
      write_length(position, length);
      uint8_t* data = std::next(this->data_, static_cast<int64_t>(position + sizeof(message_length_t)));
      std::copy_n(message.data(), length, data);  // write message bytes
      return required_space;
    } else {
      return 0;
    }
  }

  /// @brief Returns required space in bytes for allocating an object of type A in the buffer.
  /// @tparam A -- type of the message.
  /// @return total space in the buffer required fo allocating an object of type A in the buffer.
  template <class A>
    requires(sizeof(A) <= std::numeric_limits<message_length_t>::max())
  [[nodiscard]] static constexpr auto required() -> std::size_t {
    return sizeof(message_length_t) + sizeof(A);
  }

  /// @brief Allocates a message directly in the buffer.
  /// @tparam A -- type of the message.
  /// @param position -- the zero-based byte offset at which the message should be allocated in the buffer.
  /// @return std::optional<A*> none when message could not be allocated because there is not enough space.
  template <class A>
    requires(
        std::default_initializable<A> && sizeof(A) <= std::numeric_limits<message_length_t>::max() &&
        sizeof(message_length_t) + sizeof(A) <= L
    )
  [[nodiscard]] auto allocate(const size_t position) -> std::optional<A*> {
    constexpr size_t required_space = sizeof(message_length_t) + sizeof(A);

    if (this->remaining(position) >= required_space) {
      write_length(position, sizeof(A));
      uint8_t* data = std::next(this->data_, static_cast<int64_t>(position + sizeof(message_length_t)));
      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
      A* a = new (data) A{};  // construct A at the specified buffer, aka placement new

      return {a};
    } else {
      return std::nullopt;
    }
  }

  /// @param position -- zero-based byte offset.
  /// @return the available number of byte at the provided position.
  [[nodiscard]] auto remaining(const size_t position) const noexcept -> size_t {
    if (L <= position) {
      return 0;
    } else {
      return L - position;
    }
  }

  /// @brief Reads the message at the specified position. The behavior is undefined when wrong position is provided,
  ///        position has to point to a 2-byte length value that precedes the bytes data.
  /// @param position zero-based byte offset.
  /// @return the message at the provided position.
  [[nodiscard]] auto read(const size_t position) const noexcept -> span<uint8_t> {
    if (this->remaining(position) < sizeof(message_length_t)) {
      return {};
    } else {
      const message_length_t length = this->read_length(position);
      const auto data_position = static_cast<int64_t>(position + sizeof(message_length_t));
      uint8_t* data = std::next(this->data_, data_position);
      return {data, length};
    }
  }

  /// @brief Reads a message at the specified position and `reinterpret_cast`s it to the specified type.
  /// @tparam A -- type of the object.
  /// @param position -- the zero-based byte offset at which the message should be allocated in the buffer.
  /// @return std::optional<A*> none when message could not be read because there is not enough space.
  template <class A>
    requires(std::default_initializable<A> && sizeof(message_length_t) + sizeof(A) <= L)
  [[nodiscard]] auto read_unsafe(const size_t position) const noexcept -> std::optional<const A*> {
    span<uint8_t> raw = this->read(position);
    if (raw.empty()) {
      return std::nullopt;
    } else {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      return reinterpret_cast<A*>(raw.data());
    }
  }

#ifdef UNIT_TEST
  auto data() const noexcept -> span<uint8_t, L> { return span<uint8_t, L>{this->data_, L}; }
#endif

 private:
  auto write_length(const size_t position, const size_t length) noexcept -> void {
    uint8_t* data = std::next(this->data_, static_cast<int64_t>(position));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* length_data = reinterpret_cast<message_length_t*>(data);
    *length_data = static_cast<message_length_t>(length);
  }

  [[nodiscard]] auto read_length(const size_t position) const noexcept -> message_length_t {
    uint8_t* data = std::next(this->data_, static_cast<int64_t>(position));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* length_data = reinterpret_cast<message_length_t*>(data);
    const message_length_t length = *length_data;
    return length;
  }

  uint8_t* data_;
};

}  // namespace lshl::demux::core
