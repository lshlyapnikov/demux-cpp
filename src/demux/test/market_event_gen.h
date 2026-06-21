#pragma once

#include <rapidcheck.h>
#include <cstdint>

#include "../example/market_event.h"

namespace rc {

using lshl::demux::example::MarketDataUpdate;
using lshl::demux::example::MarketEvent;
using lshl::demux::example::MarketTradeUpdate;
using lshl::demux::example::Side;

template <>
struct Arbitrary<Side> {
  static auto arbitrary() -> Gen<Side> { return gen::element(Side::Bid, Side::Ask); }
};

template <>
struct Arbitrary<MarketDataUpdate> {
  static auto arbitrary() -> Gen<MarketDataUpdate> {
    return gen::build<MarketDataUpdate>(
        gen::set(&MarketDataUpdate::instrument_id),
        gen::set(&MarketDataUpdate::side),
        gen::set(&MarketDataUpdate::price),
        gen::set(&MarketDataUpdate::size),
        gen::set(&MarketDataUpdate::level),
        gen::set(&MarketDataUpdate::timestamp)
    );
  }
};

template <>
struct Arbitrary<MarketTradeUpdate> {
  static auto arbitrary() -> Gen<MarketTradeUpdate> {
    return gen::build<MarketTradeUpdate>(
        gen::set(&MarketTradeUpdate::instrument_id),
        gen::set(&MarketTradeUpdate::side),
        gen::set(&MarketTradeUpdate::price),
        gen::set(&MarketTradeUpdate::size),
        gen::set(&MarketTradeUpdate::trade_id),
        gen::set(&MarketTradeUpdate::timestamp)
    );
  }
};

template <>
struct Arbitrary<MarketEvent> {
  static auto arbitrary() -> Gen<MarketEvent> {
    return gen::oneOf(
        gen::cast<MarketEvent>(gen::arbitrary<MarketDataUpdate>()),
        gen::cast<MarketEvent>(gen::arbitrary<MarketTradeUpdate>())
    );
  }
};

}  // namespace rc