// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#include "./market_event.h"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <variant>

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
  os << "MarketDataUpdate{instrument_id: " << md.instrument_id << ", side: " << md.side << ", price: " << md.price
     << ", size: " << md.size << ", level: " << static_cast<uint32_t>(md.level)
     << ", exchange_timestamp: " << md.timestamp << "}";
  return os;
}

auto operator<<(std::ostream& os, const MarketTradeUpdate& md) -> std::ostream& {
  os << "MarketTradeUpdate{instrument_id: " << md.instrument_id << ", side: " << md.side << ", price: " << md.price
     << ", size: " << md.size << ", trade_id: " << md.trade_id << ", exchange_timestamp: " << md.timestamp << "}";
  return os;
}

auto operator<<(std::ostream& os, const MarketEvent& md) -> std::ostream& {
  std::visit(
      VisitorOverloads{
          [&os](const MarketDataUpdate& mdu) { os << mdu; },
          [&os](const MarketTradeUpdate& mtu) { os << mtu; },
      },
      md
  );
  return os;
}

auto MarketDataUpdateGenerator::generate_market_data_update(MarketDataUpdate* output) noexcept -> void {
  output->instrument_id = this->distU32_(engine_);
  output->side = this->generate_side_();
  output->level = generate_level_();
  output->price = generate_price_();
  output->size = generate_size_();

  const std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> now =
      std::chrono::steady_clock::now();
  const uint64_t x = static_cast<uint64_t>(now.time_since_epoch().count());
  output->timestamp = x;
}

inline auto MarketDataUpdateGenerator::generate_side_() noexcept -> Side {
  const uint32_t x = distU32_(engine_) % 2;
  return static_cast<Side>(x);
}

inline auto MarketDataUpdateGenerator::generate_level_() noexcept -> uint8_t {
  return this->distU8_(engine_);
}

inline auto MarketDataUpdateGenerator::generate_price_() noexcept -> uint64_t {
  return MarketDataUpdateGenerator::PRICE_MULTIPLIER * (distU32_(engine_) % std::numeric_limits<uint16_t>::max());
}

inline auto MarketDataUpdateGenerator::generate_size_() noexcept -> uint32_t {
  return MarketDataUpdateGenerator::SIZE_MULTIPLIER * (distU32_(engine_) % std::numeric_limits<uint16_t>::max());
}

}  // namespace lshl::demux::example