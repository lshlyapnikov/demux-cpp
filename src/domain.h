#ifndef __SHM_SEQUENCER_DOMAIN_H__
#define __SHM_SEQUENCER_DOMAIN_H__

#include <array>
#include <atomic>
#include <cstdint>

using std::size_t;
using std::uint16_t;
using std::uint64_t;
using std::uint8_t;

namespace ShmSequencer {

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
  const uint16_t id;
  UpstreamPacket<N> upstream_packet;
};

// struct RequestPacket {
//   Header header;
// };

}  // namespace ShmSequencer

#endif  // __SHM_SEQUENCER_DOMAIN_H__