#include "./market_data.h"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include "market_data.h"

namespace lshl::demux::example {

using std::uint32_t;
using std::uint64_t;
using std::uint8_t;

auto operator<<(std::ostream& os, const Side& side) -> std::ostream& {
  switch (side) {
    case Side::Bid:
      os << "Bid";
      break;
    case Side::Ask:
      os << "Ask";
      break;
  }
  return os;
}

auto operator<<(std::ostream& os, const MarketDataUpdate& md) -> std::ostream& {
  os << "T:" << md.timestamp << '|' << md.side << " ID:" << md.instrument_id << ' ' << md.size << " x " << md.price;
  return os;
}

auto MarketDataUpdateGenerator::generate_market_data_update(MarketDataUpdate* output) -> void {
  const std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> now =
      std::chrono::steady_clock::now();
  const uint64_t x = static_cast<uint64_t>(now.time_since_epoch().count());
  output->timestamp = x;
  output->instrument_id = this->distU32_(engine_);
  output->side = this->generate_side_();
  output->level = generate_level_();
  output->price = generate_price_();
  output->size = generate_size_();
}

inline auto MarketDataUpdateGenerator::generate_side_() -> Side {
  const uint32_t x = distU32_(engine_) % 2;
  return static_cast<Side>(x);
}

inline auto MarketDataUpdateGenerator::generate_level_() -> uint8_t {
  return this->distU8_(engine_);
}

inline auto MarketDataUpdateGenerator::generate_price_() -> uint64_t {
  return MarketDataUpdateGenerator::PRICE_MULTIPLIER * (distU32_(engine_) % std::numeric_limits<uint16_t>::max());
}

inline auto MarketDataUpdateGenerator::generate_size_() -> uint32_t {
  return MarketDataUpdateGenerator::SIZE_MULTIPLIER * (distU32_(engine_) % std::numeric_limits<uint16_t>::max());
}

}  // namespace lshl::demux::example
