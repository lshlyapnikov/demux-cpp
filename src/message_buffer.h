#ifndef DEMUX_CPP_LSHL_DEMUX_MESSAGE_BUFFER_H
#define DEMUX_CPP_LSHL_DEMUX_MESSAGE_BUFFER_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace lshl::demux {

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
  [[nodiscard]] auto write(const size_t position, const span<uint8_t> message) noexcept -> size_t {
    const size_t x = sizeof(message_length_t);
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

 private:
  uint8_t* data_;
};

}  // namespace lshl::demux

#endif  // DEMUX_CPP_LSHL_DEMUX_MESSAGE_BUFFER_H