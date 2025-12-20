// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

#define UNIT_TEST

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <iostream>
#include <variant>
#include <vector>
#include "../example/market_event.h"

TEST(EmplaceVariantTest, Basic) {
  using VarType = std::variant<int, std::string, double>;

  std::vector<VarType> vec;
  vec.emplace_back(42);
  vec.emplace_back("Hello, World!");
  vec.emplace_back(3.14);

  ASSERT_EQ(std::get<int>(vec[0]), 42);
  ASSERT_EQ(std::get<std::string>(vec[1]), "Hello, World!");
  ASSERT_EQ(std::get<double>(vec[2]), 3.14);
}

TEST(EmplaceVariantTest, MarketEvent) {
  namespace example = lshl::demux::example;

  std::vector<example::MarketEvent> events;
  events.emplace_back(example::MarketDataUpdate{1L, example::Side::Bid, 1000, 10, 1, 123456789});
  events.emplace_back(example::MarketTradeUpdate{2L, example::Side::Ask, 1010, 5, 11223344, 987654321});

  for (const auto& event : events) {
    std::cout << event << "\n";
  }
}

TEST(EmplaceVariantTest, SizeOfVariant) {
  namespace example = lshl::demux::example;
  ASSERT_GE(sizeof(example::MarketEvent), sizeof(example::MarketDataUpdate));
  ASSERT_GE(sizeof(example::MarketEvent), sizeof(example::MarketTradeUpdate));

  std::cout << "Size of MarketEvent variant: " << sizeof(example::MarketEvent) << " bytes\n";
  std::cout << "Size of MarketDataUpdate: " << sizeof(example::MarketDataUpdate) << " bytes\n";
  std::cout << "Size of MarketTradeUpdate: " << sizeof(example::MarketTradeUpdate) << " bytes\n";

  std::cout << "Align of MarketEvent: " << alignof(example::MarketEvent) << "\n";
  std::cout << "Align of MarketDataUpdate: " << alignof(example::MarketDataUpdate) << "\n";
  std::cout << "Align of MarketTradeUpdate: " << alignof(example::MarketTradeUpdate) << "\n";
}

// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)