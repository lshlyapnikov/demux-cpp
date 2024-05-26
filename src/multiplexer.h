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

  /// @brief Writes the entire passed message into the buffer.
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
  requires(N >= M + 2)
class MultiplexerPublisher {
 public:
  MultiplexerPublisher(uint8_t total_subs_number) noexcept(false)
      : total_subs_number_(validate_subscriber_number(total_subs_number)),
        all_subs_mask_(SubscriberId::all_subscribers_mask(total_subs_number)) {}

  // auto send(const span<uint8_t> source) noexcept -> bool {
  //   const size_t n = source.size();
  //   if (n == 0) {
  //     return true;
  //   } else if (n > M) {
  //     return false;
  //   } else {
  //     const size_t i = this->index_;
  //     std::atomic<uint32_t>& mask = this->masks_[i];
  //     wait_exact_value_and_acquire(mask, this->all_subscribers_mask_);
  //     std::copy(source.begin(), source.end(), this->buffers_[i]);
  //     this->buffer_lengths_[i] = n;
  //     set_and_release(&mask, 0U);
  //     this->increment_index();
  //     return true;
  //   }
  // }

  auto send(const span<uint8_t> source) noexcept -> bool {
    const size_t n = source.size();
    if (n == 0) {
      return true;
    } else if (n > M) {
      return false;
    } else {
      const size_t required_space = sizeof(uint16_t) + n;
      const size_t cur_position = this->size_;
      const size_t new_position = cur_position + required_space;
      if (new_position <= N) {
        uint8_t* dst_address = this->buffer_.data() + cur_position;
        // write the source length
        const uint16_t length = static_cast<uint16_t>(n);
        memcpy(dst_address, &length, sizeof(uint16_t));
        // write the source bytes
        dst_address += sizeof(uint16_t);
        // std::copy(source.begin(), source.end(), dst_address);
        memcpy(dst_address, source.data(), n);
        this->size_ = new_position;
        message_count_.fetch_add(1L, std::memory_order_seq_cst);
        return true;
      } else {
        this->all_subs_reached_end_of_buffer_mask_.store(0L);
        this->size_ = 0;
        // wait for all subs to reach the end of buffer.
        return false;
      }
    }
  }

  auto wait_all_subs_reached_end_of_buffer() const noexcept -> void {
    while (true) {
      const uint32_t x = this->all_subs_reached_end_of_buffer_mask_.load();
      if (x == this->all_subs_mask_) {
        return;
      }
    }
  }

  auto message_count() const noexcept -> size_t { return this->message_count_.load(); }

  auto data() const noexcept -> span<uint8_t> { return span(this->buffers_.data(), this->size_); }

  /// new design
  /// roll over if there is no remaining space in the buffer or available space is less than 2 bytes.
  auto should_roll_over() -> bool {
    // TODO: use MessageBuffer::remaining method here
    return this->size_ >= N || (N - this->size_) < sizeof(uint16_t);
  }

 private:
  auto increment_index() noexcept -> void {
    const size_t i = this->index_;
    this->index_ = (i == N - 1) ? 0 : i + 1;
  }

  size_t size_{0};
  std::atomic<uint64_t> message_count_{0};
  std::array<uint8_t, N> buffer_;

  std::atomic<size_t> position_{0};

  uint8_t total_subs_number_;
  const uint32_t all_subs_mask_;
  std::array<std::atomic<uint32_t>, N> all_subs_reached_end_of_buffer_mask_;
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