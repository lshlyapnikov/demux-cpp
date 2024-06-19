#ifndef LSHL_DEMUX_EXAMPLE_MARKET_DATA_H
#define LSHL_DEMUX_EXAMPLE_MARKET_DATA_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>

namespace lshl::demux::example {

using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

enum class Side { Bid, Ask };

auto operator<<(std::ostream& os, const Side& side) -> std::ostream&;

struct MarketDataUpdate {
  uint64_t timestamp;
  uint32_t instrument_id;
  Side side;
  uint8_t level;
  uint64_t price;
  uint32_t size;
};

auto operator<<(std::ostream& os, const MarketDataUpdate& md) -> std::ostream&;

class MarketDataUpdateGenerator {
 public:
  auto generate_market_data_update(MarketDataUpdate* output) -> void;

 private:
  auto generate_side_() -> Side;
  auto generate_level_() -> uint8_t;
  auto generate_price_() -> uint64_t;
  auto generate_size_() -> uint32_t;

  std::chrono::steady_clock clock_;
  std::mt19937 engine_;
  std::uniform_int_distribution<uint32_t> distU32_;
  std::uniform_int_distribution<uint8_t> distU8_;
};

}  // namespace lshl::demux::example

#endif  // LSHL_DEMUX_EXAMPLE_MARKET_DATA_H