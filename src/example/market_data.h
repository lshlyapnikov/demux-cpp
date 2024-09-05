// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#ifndef LSHL_DEMUX_EXAMPLE_MARKET_DATA_H
#define LSHL_DEMUX_EXAMPLE_MARKET_DATA_H

#include <cstdint>
#include <iostream>
#include <random>

namespace lshl::demux::example {

using std::uint16_t;
using std::uint32_t;
using std::uint64_t;

enum class Side : std::uint8_t { Bid, Ask };

auto operator<<(std::ostream& os, const Side& side) -> std::ostream&;

// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
struct MarketDataUpdate {
  MarketDataUpdate() = default;   // constructor
  ~MarketDataUpdate() = default;  // destructor
  // guarantees that it is only passed by reference or via a pointer
  MarketDataUpdate(const MarketDataUpdate&) = delete;                         // copy constructor
  auto operator=(const MarketDataUpdate&) -> MarketDataUpdate& = delete;      // copy assignment
  MarketDataUpdate(MarketDataUpdate&&) noexcept = delete;                     // move constructor
  auto operator=(MarketDataUpdate&&) noexcept -> MarketDataUpdate& = delete;  // move assignment

  uint64_t timestamp;
  uint32_t instrument_id;
  Side side;
  uint8_t level;
  uint64_t price;
  uint32_t size;
};
// NOLINTEND(misc-non-private-member-variables-in-classes)

auto operator<<(std::ostream& os, const MarketDataUpdate& md) -> std::ostream&;

class MarketDataUpdateGenerator {
 public:
  auto generate_market_data_update(MarketDataUpdate* output) -> void;

  static constexpr uint64_t PRICE_MULTIPLIER = 1000000000;
  static constexpr uint32_t SIZE_MULTIPLIER = 100;

 private:
  auto generate_side_() -> Side;
  auto generate_level_() -> uint8_t;
  auto generate_price_() -> uint64_t;
  auto generate_size_() -> uint32_t;

  std::mt19937 engine_;
  std::uniform_int_distribution<uint32_t> distU32_;
  std::uniform_int_distribution<uint8_t> distU8_;
};

}  // namespace lshl::demux::example

#endif  // LSHL_DEMUX_EXAMPLE_MARKET_DATA_H