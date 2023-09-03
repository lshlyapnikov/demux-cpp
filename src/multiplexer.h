#ifndef __SHM_SEQUENCER_MULTIPLEXER_H__
#define __SHM_SEQUENCER_MULTIPLEXER_H__

#include <cassert>
#include <cstring>
#include "./domain.h"

/// @brief Packet. Access has to be synchronized with an external atomic guard.
/// @tparam M max packet size.
template <const uint16_t M>
class Packet {
 public:
  auto write(const std::array<uint16_t, M>& src, uint16_t src_size) noexcept -> void {
    assert(src_size <= M);
    this->size = src_size;
    std::memcpy(this->buffer.data, src.data, src_size);
    this->size = src_size;
  }

  [[nodiscard]] auto read(std::array<uint16_t, M>& dst) const noexcept -> uint16_t {
    std::memcpy(dst.data, this->buffer.data, this->size);
    return this->size;
  }

 private:
  uint16_t size{0};
  std::array<uint8_t, M> buffer;
};

/// @brief Multiplexer publisher.
/// @tparam N max number of packets.
/// @tparam M max packet size.
template <const size_t N, const uint16_t M>
struct MultiplexerPublisher {
  std::array<Packet<M>, N> buffer;
};

/// @brief Multiplexer subscriber.
/// @tparam N max number of packets.
/// @tparam M max packet size.
template <const size_t N, const uint16_t M>
struct MultiplexerSubscriber {
  std::array<Packet<M>, N> buffer;
};

#endif  // __SHM_SEQUENCER_MULTIPLEXER_H__