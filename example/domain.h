#ifndef LSHL_DEMUX_EXAMPLE_DOMAIN_H
#define LSHL_DEMUX_EXAMPLE_DOMAIN_H

#include <cstddef>
#include <cstdint>

namespace lshl::demux::example {

using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

enum class Side { Bid, Ask };

struct MarketDataUpdate {
  uint64_t timestamp;
  uint32_t instrument_id;
  Side side;
  uint16_t level;
  uint64_t price;
  uint32_t size;
};

}  // namespace lshl::demux::example

#endif  // LSHL_DEMUX_EXAMPLE_DOMAIN_H