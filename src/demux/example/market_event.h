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

class MarketDataUpdate {
 public:
  MarketDataUpdate(
      uint32_t instrument_id,
      Side side,
      uint64_t price,
      uint32_t size,
      uint8_t level,
      uint64_t exchange_timestamp
  )
      : instrument_id_(instrument_id),
        side_(side),
        price_(price),
        size_(size),
        level_(level),
        exchange_timestamp_(exchange_timestamp) {}
  ~MarketDataUpdate() = default;

  MarketDataUpdate(const MarketDataUpdate&) = delete;
  auto operator=(const MarketDataUpdate&) -> MarketDataUpdate& = delete;

  MarketDataUpdate(MarketDataUpdate&&) noexcept = default;
  auto operator=(MarketDataUpdate&&) noexcept -> MarketDataUpdate& = default;

  [[nodiscard]] auto instrument_id() const noexcept -> uint32_t { return instrument_id_; }
  [[nodiscard]] auto side() const noexcept -> Side { return side_; }
  [[nodiscard]] auto price() const noexcept -> uint64_t { return price_; }
  [[nodiscard]] auto size() const noexcept -> uint32_t { return size_; }
  [[nodiscard]] auto level() const noexcept -> uint8_t { return level_; }
  [[nodiscard]] auto exchange_timestamp() const noexcept -> uint64_t { return exchange_timestamp_; }

 private:
  uint32_t instrument_id_;
  Side side_;
  uint64_t price_;
  uint32_t size_;
  uint8_t level_;
  uint64_t exchange_timestamp_;
};

auto operator<<(std::ostream& os, const MarketDataUpdate& md) -> std::ostream&;

class MarketTradeUpdate {
 public:
  MarketTradeUpdate(
      uint32_t instrument_id,
      Side side,
      uint64_t price,
      uint32_t size,
      uint64_t trade_id,
      uint64_t exchange_timestamp
  )
      : instrument_id_(instrument_id),
        side_(side),
        price_(price),
        size_(size),
        trade_id_(trade_id),
        exchange_timestamp_(exchange_timestamp) {}
  ~MarketTradeUpdate() = default;

  MarketTradeUpdate(const MarketTradeUpdate&) = delete;
  auto operator=(const MarketTradeUpdate&) -> MarketTradeUpdate& = delete;

  MarketTradeUpdate(MarketTradeUpdate&&) noexcept = default;
  auto operator=(MarketTradeUpdate&&) noexcept -> MarketTradeUpdate& = default;

  [[nodiscard]] auto instrument_id() const noexcept -> uint32_t { return instrument_id_; }
  [[nodiscard]] auto side() const noexcept -> Side { return side_; }
  [[nodiscard]] auto price() const noexcept -> uint64_t { return price_; }
  [[nodiscard]] auto size() const noexcept -> uint32_t { return size_; }
  [[nodiscard]] auto trade_id() const noexcept -> uint64_t { return trade_id_; }
  [[nodiscard]] auto exchange_timestamp() const noexcept -> uint64_t { return exchange_timestamp_; }

 private:
  uint32_t instrument_id_;
  Side side_;
  uint64_t price_;
  uint32_t size_;
  uint64_t trade_id_;
  uint64_t exchange_timestamp_;
};

auto operator<<(std::ostream& os, const MarketTradeUpdate& md) -> std::ostream&;

using MarketEvent = std::variant<MarketDataUpdate, MarketTradeUpdate>;

auto operator<<(std::ostream& os, const MarketEvent& md) -> std::ostream&;

}  // namespace lshl::demux::example