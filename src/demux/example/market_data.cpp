// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#include "./market_data.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include "../util/timestamp_util.h"
#include "market_data.h"

namespace {
using std::uint32_t;
using std::uint64_t;
using std::uint8_t;

using lshl::demux::example::Side;

auto generate_side_(uint64_t rnd) -> Side {
  return static_cast<Side>(rnd % 2);
}

auto generate_level_(uint64_t rnd) -> uint8_t {
  return static_cast<uint8_t>(rnd % std::numeric_limits<uint8_t>::max());
}

auto generate_price_(uint64_t rnd) -> uint64_t {
  return lshl::demux::example::PRICE_MULTIPLIER * (rnd % std::numeric_limits<uint16_t>::max());
}

auto generate_size_(uint64_t rnd) -> uint32_t {
  return lshl::demux::example::SIZE_MULTIPLIER * (rnd % std::numeric_limits<uint16_t>::max());
}
}  // namespace

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

auto generate_market_data_update(MarketDataUpdate* output) -> void {
  const uint64_t x = util::monotonic_timestamp_ns();
  output->timestamp = x;
  output->instrument_id = x % std::numeric_limits<uint32_t>::max();
  output->side = generate_side_(x);
  output->level = generate_level_(x);
  output->price = generate_price_(x);
  output->size = generate_size_(x);
}
}  // namespace lshl::demux::example
