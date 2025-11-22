// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace lshl::demux::util {

template <typename T>
concept OutputStreamConcept = requires(T os) {
  { os << std::declval<std::string>() };  // Stream should support the << operator for strings
};

template <OutputStreamConcept OutputStream>
auto operator<<(OutputStream& os, const std::span<uint8_t>& xs) -> OutputStream& {
  os << "hex:" << std::hex << '[';
  bool first = true;
  for (const uint8_t x : xs) {
    if (first) {
      first = false;
    } else {
      os << ' ';
    }
    os << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(x);
  }
  os << ']' << std::dec;
  return os;
}

template <OutputStreamConcept OutputStream>
auto operator<<(OutputStream& os, const std::vector<uint8_t>& xs) -> OutputStream& {
  os << "hex:" << std::hex << '[';
  bool first = true;
  for (const uint8_t x : xs) {
    if (first) {
      first = false;
    } else {
      os << ' ';
    }
    os << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(x);
  }
  os << ']' << std::dec;
  return os;
}

template <OutputStreamConcept OutputStream, size_t M>
auto operator<<(OutputStream& os, const std::array<uint8_t, M>& xs) -> OutputStream& {
  std::array<uint8_t, M> ys{xs};
  return operator<<(os, std::span{ys});
}

template <OutputStreamConcept OutputStream, typename T>
auto operator<<(OutputStream& os, const std::vector<T>& xs) -> OutputStream& {
  os << '[';
  bool first = true;
  for (const auto& x : xs) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    os << x;
  }
  os << ']';
  return os;
}

}  // namespace lshl::demux::util
