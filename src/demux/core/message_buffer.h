// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#ifndef DEMUX_CPP_LSHL_DEMUX_MESSAGE_BUFFER_H
#define DEMUX_CPP_LSHL_DEMUX_MESSAGE_BUFFER_H

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace lshl::demux::core {

using std::span;
using std::uint16_t;
using std::uint8_t;

/// @brief MessageBuffer wraps a byte array, which can be allocated in shared memory. MessageBuffer does not take the
/// ownership of the passed buffer. The outside logic is responsible to freeing the passed buffer. External
/// synchronization is required for accessing the `data_` via `read` and `write` calls. Read or write `position` must be
/// stored outside of this class.
/// @tparam L total buffer size in bytes.
template <size_t L>
struct MessageBuffer {
  using message_length_t = uint16_t;

 public:
  explicit MessageBuffer(span<uint8_t, L> buffer) : data_(buffer.data()) {}

  /// @brief Writes the entire passed message into the buffer or nothing.
  /// @param position -- the zero-based byte offset at which the message should be written.
  /// @param message -- the bytes that should be written.
  /// @return the total number of written bytes (2 + message length) or zero.
  [[nodiscard]] auto write(const size_t position, const span<uint8_t>& message) noexcept -> size_t {
    constexpr size_t x = sizeof(message_length_t);
    const size_t n = message.size();
    const size_t total_required = n + x;

    if (this->remaining(position) >= total_required) {
      // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      uint8_t* data = this->data_;
      std::copy_n(&n, x, data + position);                  // write message length
      std::copy_n(message.data(), n, data + position + x);  // write message bytes
      // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      return total_required;
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
    requires(std::default_initializable<A> && sizeof(A) <= std::numeric_limits<message_length_t>::max() &&
             sizeof(message_length_t) + sizeof(A) <= L)
  [[nodiscard]] auto allocate(const size_t position) -> std::optional<A*> {
    constexpr size_t x = sizeof(message_length_t);
    constexpr size_t n = sizeof(A);
    constexpr size_t total_required = n + x;

    if (this->remaining(position) >= total_required) {
      // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      uint8_t* data = this->data_;
      std::copy_n(&n, x, data + position);    // write message length
      uint8_t* buffer = data + position + x;  // start of the message bytes
      // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

      // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
      A* a = new (buffer) A{};  // construct A at the specified buffer, aka placement new

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
      // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      message_length_t msg_size = 0;
      uint8_t* data = this->data_;
      data += position;
      std::copy_n(data, sizeof(message_length_t), &msg_size);
      data += sizeof(message_length_t);
      // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      return {data, msg_size};
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
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-type-const-cast, modernize-use-auto)
      return reinterpret_cast<A*>(raw.data());
    }
  }

 private:
  uint8_t* data_;
};

}  // namespace lshl::demux::core

#endif  // DEMUX_CPP_LSHL_DEMUX_MESSAGE_BUFFER_H