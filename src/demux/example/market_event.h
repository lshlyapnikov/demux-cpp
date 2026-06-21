// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <variant>

namespace lshl::demux::example {

using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::variant;

constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;

// helper type for the visitor, see std::visit documentation
template <class... Ts>
struct VisitorOverloads : Ts... {
  using Ts::operator()...;
};

enum class Side : std::uint8_t { Bid, Ask };

auto operator<<(std::ostream& os, const Side& side) -> std::ostream&;

// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
struct alignas(CACHE_LINE_SIZE) MarketDataUpdate {
  uint32_t instrument_id;
  Side side;
  uint64_t price;
  uint32_t size;
  uint8_t level;
  uint64_t timestamp;

  auto operator<=>(const MarketDataUpdate&) const = default;
};
// NOLINTEND(misc-non-private-member-variables-in-classes)

auto operator<<(std::ostream& os, const MarketDataUpdate& md) -> std::ostream&;

// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
struct alignas(CACHE_LINE_SIZE) MarketTradeUpdate {
  uint32_t instrument_id;
  Side side;
  uint64_t price;
  uint32_t size;
  uint64_t trade_id;
  uint64_t timestamp;

  auto operator<=>(const MarketTradeUpdate&) const = default;
};
// NOLINTEND(misc-non-private-member-variables-in-classes)

auto operator<<(std::ostream& os, const MarketTradeUpdate& md) -> std::ostream&;

using MarketEvent = std::variant<MarketDataUpdate, MarketTradeUpdate>;

auto operator<<(std::ostream& os, const MarketEvent& md) -> std::ostream&;

struct FastRng {
 public:
  using result_type = uint64_t;

  static constexpr auto min() noexcept -> uint64_t { return 0; }
  static constexpr auto max() noexcept -> uint64_t { return std::numeric_limits<uint64_t>::max(); }

  auto operator()() noexcept -> uint64_t {
    // NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    // NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
    return state;
  }

 private:
  uint64_t state;
};

class MarketDataUpdateGenerator {
 public:
  auto generate_market_data_update(MarketDataUpdate* output) noexcept -> void;

  static constexpr uint64_t PRICE_MULTIPLIER = 1000000000;
  static constexpr uint32_t SIZE_MULTIPLIER = 100;

 private:
  auto generate_side_() noexcept -> Side;
  auto generate_level_() noexcept -> uint8_t;
  auto generate_price_() noexcept -> uint64_t;
  auto generate_size_() noexcept -> uint32_t;

  FastRng engine_{};
  std::uniform_int_distribution<uint32_t> distU32_;
  std::uniform_int_distribution<uint8_t> distU8_;
};

}  // namespace lshl::demux::example