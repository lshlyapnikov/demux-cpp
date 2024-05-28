#ifndef SHM_SEQUENCER_MULTIPLEXER_H
#define SHM_SEQUENCER_MULTIPLEXER_H
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include "atomic_util.h"
#include "domain.h"

namespace ShmSequencer {

using std::array;
using std::atomic;
using std::make_unique;
using std::nullopt;
using std::optional;
using std::size_t;
using std::span;
using std::uint64_t;
using std::unique_ptr;

/// @brief MessageBuffer wraps a byte array, which can be allocated in shared memory. MessageBuffer does not take the
/// ownership of the passed buffer. The outside logic is responsible to freeing the passed buffer. External
/// synchronization is required for accessing the `data_` via `read` and `write` calls. Read or write `position` must be
/// stored outside of this class.
struct MessageBuffer {
  using message_length_t = uint16_t;

 public:
  MessageBuffer(span<uint8_t> buffer) : size_(buffer.size()), data_(buffer.data()) {}

  /// @brief Writes the entire passed message into the buffer or nothing.
  /// @param position -- the zero-based byte offset at which the message should be written.
  /// @param message -- the bytes that should be written.
  /// @return the total number of written bytes (2 + message length) or zero.
  auto write(const size_t position, const span<uint8_t> message) noexcept -> size_t {
    const size_t x = sizeof(message_length_t);
    const size_t n = message.size();
    const size_t total_required = n + x;

    if (this->remaining(position) >= total_required) {
      uint8_t* data = this->data_;
      std::copy_n(&n, x, data + position);                  // write message length
      std::copy_n(message.data(), n, data + position + x);  // write message bytes
      return total_required;
    } else {
      return 0;
    }
  }

  /// @param position -- zero-based byte offset.
  /// @return the available number of byte at the provided position.
  auto remaining(const size_t position) const noexcept -> size_t {
    if (this->size_ <= position) {
      return 0;
    } else {
      return this->size_ - position;
    }
  }

  /// @brief Reads the message at the specified position. The behavior is undefined when wrong position is provided,
  ///        position has to point to a 2-byte length value that precedes the bytes data.
  /// @param position zero-based byte offset.
  /// @return the message at the provided position.
  auto read(const size_t position) const noexcept -> span<uint8_t> {
    message_length_t msg_size = 0;
    uint8_t* data = this->data_;
    data += position;
    std::copy_n(data, sizeof(message_length_t), &msg_size);
    data += sizeof(message_length_t);
    return span(data, msg_size);
  }

 private:
  size_t const size_;
  uint8_t* const data_;
};

// TODO: Leo: This is unfinished
// TODO: make sure the size of the shares memory is a multiple of the page size. Because the operating system
// TODO: performs mapping operations over whole pages. So, you don't waste memory.
//
/// @brief Multiplexer publisher.
/// @tparam N total buffer size in bytes.
/// @tparam M max packet size in bytes.
template <const size_t N, const uint16_t M>
  requires(N >= M + 2 && M > 0)
class MultiplexerPublisher {
 public:
  MultiplexerPublisher(uint8_t total_subs_number,
                       span<uint8_t, N> buffer,
                       atomic<uint64_t>* message_count_sync,
                       atomic<uint64_t>* wraparound_sync) noexcept(false)
      : total_subs_number_(validate_subscriber_number(total_subs_number)),
        all_subs_mask_(SubscriberId::all_subscribers_mask(total_subs_number)),
        buffer_(buffer),
        message_count_sync_(message_count_sync),
        wraparound_sync_(wraparound_sync) {}

  auto send(const span<uint8_t> source) noexcept(false) -> bool {
    const size_t k = source.size();
    if (0 < k && k <= M) {
      return this->send_(source);
    } else {
      throw std::invalid_argument(std::string("Must have 0 < source.size() <= M. Got source.size(): ") +
                                  std::to_string(k) + ", M: " + std::to_string(M));
    }
  }

  template <const uint16_t K>
    requires(K <= M && K > 0)
  auto send(const span<uint8_t, K> source) noexcept -> bool {
    return this->send_(source);
  }

  auto data() const noexcept -> span<uint8_t> { return span(this->buffers_.data(), this->position_); }

  /// new design
  /// roll over if there is no remaining space in the buffer or available space is less than 2 bytes.
  auto should_roll_over() -> bool {
    // TODO: use MessageBuffer::remaining method here
    return this->position_ >= N || (N - this->position_) < sizeof(uint16_t);
  }

 private:
  auto send_(const span<uint8_t> source) noexcept -> bool;

  auto wait_for_subs_to_catch_up_and_wraparound_() const noexcept -> void;

  auto increment_message_count_() noexcept -> void;

  const uint8_t total_subs_number_;
  const uint64_t all_subs_mask_;

  size_t position_{0};
  uint64_t message_count_{0};
  MessageBuffer const buffer_;
  atomic<uint64_t>* const message_count_sync_;
  atomic<uint64_t>* const wraparound_sync_;
};

// TODO: Leo: This is unfinished
//
/// @brief Multiplexer subscriber. Should be mapped into shared memory allocated by MultiplexerPublisher.
/// @tparam N max number of packets.
/// @tparam M max packet size.
template <const size_t N, const uint16_t M>
class MultiplexerSubscriber {
  // auto receive(Packet<M>* packet) noexcept -> bool {
  //   const size_t required_space = sizeof(packet->size) + packet->size;
  //   const size_t new_offset = this->offset + required_space;
  //   if (new_offset <= BUFFER_SIZE) {
  //     void* dst_address = this->buffer.data + this->offset;
  //     std::memcpy(dst_address, &packet, required_space);
  //     this->offset = new_offset;
  //   } else {
  //     return false;
  //   }
  // }

  // auto receive() noexcept -> std::optional<const Packet<M>*> {
  //   // test
  //   return std::nullopt;
  // }

 private:
  static const size_t BUFFER_SIZE = M * N + 2 * N;
  std::atomic<size_t> offset_{0};
  std::array<uint8_t, BUFFER_SIZE> buffer_;

  SubscriberId id;
};

}  // namespace ShmSequencer

#endif  // SHM_SEQUENCER_MULTIPLEXER_H