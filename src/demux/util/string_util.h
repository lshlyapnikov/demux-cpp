// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <boost/lexical_cast.hpp>
#include <concepts>
#include <ranges>
#include <string_view>
#include <vector>

namespace lshl::demux::util {

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <Numeric T>
auto parse_vector(std::string_view src, char delimiter = ',') -> std::vector<T> {
  std::vector<T> dest;

  for (const auto word : std::views::split(src, delimiter)) {
    // Convert the subrange to a string_view cleanly
    std::string_view token{word.begin(), word.end()};

    if (token.empty()) {
      continue;
    }

    try {
      // Boost handles string_view to numeric conversion directly
      dest.push_back(boost::lexical_cast<T>(token));
    } catch (const boost::bad_lexical_cast&) {
      // Handle or ignore malformed tokens gracefully
      continue;
    }
  }

  return dest;
}

}  // namespace lshl::demux::util