#ifndef SHM_SEQUENCER_MULTIPLEXER_H
#define SHM_SEQUENCER_MULTIPLEXER_H
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
#include "atomic_util.h"
#include "domain.h"

namespace ShmSequencer {

using std::array;
using std::atomic;
using std::optional;
using std::size_t;
using std::span;
using std::uint64_t;

// enum MultiplexerResult { Ok, FullBuffer, MemoryOverlap };

/// @brief Multiplexer publisher.
/// @tparam N total buffer size in bytes.
/// @tparam M max packet size in bytes.
template <const size_t N, const uint16_t M>
  requires(N >= M + 2)
class MultiplexerPublisher {
 public:
  MultiplexerPublisher(uint8_t subscriber_number) noexcept(false)
      : index_(0),
        subscriber_number_(validate_subscriber_number(subscriber_number)),
        all_subscribers_mask_(SubscriberId::all_subscribers_mask(subscriber_number)) {}

  auto send(const span<uint8_t> source) noexcept -> bool {
    const size_t n = source.size();
    if (n == 0) {
      return true;
    } else if (n > M) {
      return false;
    } else {
      const size_t i = this->index_;
      std::atomic<uint32_t>& mask = this->masks_[i];
      wait_exact_value_and_acquire(mask, this->all_subscribers_mask_);
      std::copy(source.begin(), source.end(), this->buffers_[i]);
      this->buffer_lengths_[i] = n;
      set_and_release(&mask, 0U);
      this->increment_index();
      return true;
    }
  }

 private:
  auto increment_index() noexcept -> void {
    const size_t i = this->index_;
    this->index_ = (i == N - 1) ? 0 : i + 1;
  }

  array<array<uint8_t, M>, N> buffers_;
  array<size_t, N> buffer_lengths_;
  std::array<std::atomic<uint32_t>, N> masks_;
  size_t index_;
  const uint8_t subscriber_number_;
  const uint32_t all_subscribers_mask_;
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