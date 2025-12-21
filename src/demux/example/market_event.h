// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <iostream>
#include <variant>

namespace lshl::demux::example {

using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::variant;

// helper type for the visitor, see std::visit documentation
template <class... Ts>
struct VisitorOverloads : Ts... {
  using Ts::operator()...;
};

enum class Side : std::uint8_t { Bid, Ask };

auto operator<<(std::ostream& os, const Side& side) -> std::ostream&;

// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
struct MarketDataUpdate {
  uint32_t instrument_id;
  Side side;
  uint64_t price;
  uint32_t size;
  uint8_t level;
  uint64_t exchange_timestamp;
};
// NOLINTEND(misc-non-private-member-variables-in-classes)

auto operator<<(std::ostream& os, const MarketDataUpdate& md) -> std::ostream&;

// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
struct MarketTradeUpdate {
  uint32_t instrument_id;
  Side side;
  uint64_t price;
  uint32_t size;
  uint64_t trade_id;
  uint64_t exchange_timestamp;
};
// NOLINTEND(misc-non-private-member-variables-in-classes)

auto operator<<(std::ostream& os, const MarketTradeUpdate& md) -> std::ostream&;

using MarketEvent = std::variant<MarketDataUpdate, MarketTradeUpdate>;

auto operator<<(std::ostream& os, const MarketEvent& md) -> std::ostream&;

}  // namespace lshl::demux::example