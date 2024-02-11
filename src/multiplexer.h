#ifndef SHM_SEQUENCER_MULTIPLEXER_H
#define SHM_SEQUENCER_MULTIPLEXER_H

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>

namespace ShmSequencer {

using std::uint64_t;

typedef uint32_t subscriber_id_t;
// const size_t MAX_SUBSCRIBER_NUM = sizeof(subscriber_id_t) * 8;

constexpr auto subscriber_id_to_index(subscriber_id_t id) -> size_t {
  switch (id) {
    case 0b00000000000000000000000000000001:
      return 0;
    case 0b00000000000000000000000000000010:
      return 1;
    case 0b00000000000000000000000000000100:
      return 2;
    default:
      return 1;
  }
}

// enum MultiplexerResult { Ok, FullBuffer, MemoryOverlap };

/// @brief Packet. Access has to be synchronized with an external atomic guard. A packet instance belongs to a client,
/// client sends upstream commands via the packet. Packet is allocated by the client.
/// @tparam M max packet size.
template <const uint16_t M>
class Packet {
 public:
  auto read_from(const std::array<uint8_t, M>& src, const uint16_t src_size) noexcept -> void {
    assert(src_size <= M);
    std::memcpy(this->buffer_.data(), src.data(), src_size);
    // std::copy_n(std::begin(src), src_size, std::begin(this->buffer_));
    this->size_ = src_size;
  }

  [[nodiscard]] auto write_into(std::array<uint8_t, M>* dst) const noexcept -> uint16_t {
    std::memcpy(dst->data(), this->buffer_.data(), this->size_);
    // std::copy_n(std::begin(this->buffer_), this->size_, std::begin(*dst));
    return this->size_;
  }

  [[nodiscard]] auto size() const noexcept -> uint16_t { return this->size_; }

 private:
  uint16_t size_{0};
  std::array<uint8_t, M> buffer_;
};

/// @brief Multiplexer publisher.
/// @tparam N max number of packets.
/// @tparam M max packet size.
template <const size_t N, const uint16_t M>
class MultiplexerPublisher {
 public:
  /// @brief Copies the packet data into the shared buffer.
  /// @param packet that will be copied.
  /// @return true if the packet was copied and there was enough space in the buffer.
  auto send(const Packet<M>& packet) noexcept -> bool {
    if (packet.size == 0) {
      return true;
    }
    const size_t required_space = sizeof(packet.size) + packet.size;
    const size_t current_offset = this->offset_.load(std::memory_order_acquire);
    const size_t new_offset = current_offset + required_space;
    if (new_offset <= BUFFER_SIZE) {
      const uint8_t* dst_address = this->buffer_.data() + current_offset;
      std::memcpy(dst_address, &packet, required_space);
      this->offset_.store(new_offset, std::memory_order_release);  // this->offset = new_offset;
      return true;
    } else {
      return false;
    }
  }

  auto roll_over() noexcept -> void { this->offset_.store(0, std::memory_order_release); }

 private:
  static const size_t BUFFER_SIZE = M * N + 2 * N;
  std::atomic<size_t> offset_{0};
  std::array<uint8_t, BUFFER_SIZE> buffer_;
  // std::array<std::atomic<SUB_ID>> read_by_subscribers_;
  std::array<std::array<uint8_t, M>, N> buffer2_;
};

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

  auto receive() noexcept -> std::optional<const Packet<M>*> {
    // test
    return std::nullopt;
  }

 private:
  static const size_t BUFFER_SIZE = M * N + 2 * N;
  std::atomic<size_t> offset_{0};
  std::array<uint8_t, BUFFER_SIZE> buffer_;

  static const size_t x = subscriber_id_to_index(1);
};

}  // namespace ShmSequencer

#endif  // SHM_SEQUENCER_MULTIPLEXER_H