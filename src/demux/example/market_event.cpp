// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#include "./market_event.h"
#include <cstdint>
#include <iostream>
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
  os << "MarketDataUpdate{instrument_id: " << md.instrument_id() << ", side: " << md.side() << ", price: " << md.price()
     << ", size: " << md.size() << ", level: " << static_cast<uint32_t>(md.level())
     << ", exchange_timestamp: " << md.exchange_timestamp() << "}";
  return os;
}

auto operator<<(std::ostream& os, const MarketTradeUpdate& md) -> std::ostream& {
  os << "MarketTradeUpdate{instrument_id: " << md.instrument_id() << ", side: " << md.side()
     << ", price: " << md.price() << ", size: " << md.size() << ", trade_id: " << md.trade_id()
     << ", exchange_timestamp: " << md.exchange_timestamp() << "}";
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

}  // namespace lshl::demux::example