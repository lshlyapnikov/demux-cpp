// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <iostream>
#include <tuple>

namespace lshl::demux::util {

// Concept to check if a type is streamable (supports operator<<)
template <typename T>
concept Streamable = requires(std::ostream& os, T&& val) { os << std::forward<T>(val); };

// Base case for recursive printing (Index == sizeof...(Types)) - stops recursion
template <std::size_t Index = 0, typename... Types>
  requires(Index == sizeof...(Types))  // Base case: no more elements to print
auto print_tuple(std::ostream& /*unused*/, const std::tuple<Types...>& /*unused*/) -> void {
  // Do nothing, recursion stops here
}

// Helper function to print the tuple recursively
template <std::size_t Index = 0, typename... Types>
  requires(Index < sizeof...(Types))  // Ensures this is called recursively while Index < sizeof...(Types)
auto print_tuple(std::ostream& os, const std::tuple<Types...>& t) -> void {
  if constexpr (Index > 0) {
    os << ", ";
  }
  os << std::get<Index>(t);       // Print the element at the current index
  print_tuple<Index + 1>(os, t);  // Recursive call to print the next element
}

// Overload operator<< for std::tuple
template <typename... Types>
  requires(requires(std::ostream& os, const std::tuple<Types...>& t) {
            print_tuple(os, t);  // Ensure we can print the tuple
          })
auto operator<<(std::ostream& os, const std::tuple<Types...>& t) -> std::ostream& {
  os << "(";
  print_tuple(os, t);  // Call the recursive helper function to print the tuple elements
  os << ")";
  return os;
}

}  // namespace lshl::demux::util
