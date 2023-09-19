#ifndef SHM_SEQUENCER_DOMAIN_H
#define SHM_SEQUENCER_DOMAIN_H

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ShmSequencer {

using std::size_t;
using std::uint16_t;
using std::uint64_t;
using std::uint8_t;

// template <class T>
// class NewType {
//  public:
//   NewType(T value) : value_(std::move(value)) {}

//   [[nodiscard]] auto value() const noexcept -> & T { return this->value_; }

//  private:
//   T value_;
// };

// class ClientId {
//  public:
//   ClientId(uint16_t value) : value_(value) {}

//   auto value() const noexcept -> uint16_t { return this->value_; }

//  private:
//   uint16_t value_;
// };

struct Header {
  std::atomic<uint64_t> sequence_number;
  uint16_t message_count;
};

template <const size_t N>
struct DownstreamPacket {
  Header header;
  uint16_t message_length;
  std::array<uint8_t, N> message_data;
};

template <const size_t N>
struct UpstreamPacket {
  Header header;
  uint16_t message_length;
  std::array<uint8_t, N> message_data;
};

template <const size_t N>
struct Client {
  uint16_t id;
  UpstreamPacket<N> upstream_packet;
};

// struct RequestPacket {
//   Header header;
// };

}  // namespace ShmSequencer

#endif  // SHM_SEQUENCER_DOMAIN_H