// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#ifndef DEMUX_CPP_LSHL_DEMUX_READER_ID_H
#define DEMUX_CPP_LSHL_DEMUX_READER_ID_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace lshl::demux::core {

using std::size_t;
using std::uint16_t;
using std::uint64_t;
using std::uint8_t;

constexpr uint8_t MAX_READER_NUM = sizeof(uint64_t) * 8;  // 64

[[nodiscard]] constexpr auto is_valid_reader_number(uint8_t num) noexcept -> bool {
  return 1 <= num && num <= MAX_READER_NUM;
}

constexpr auto validate_reader_number(const uint8_t num) noexcept(false) -> uint8_t {
  if (is_valid_reader_number(num)) {
    return num;
  }
  throw std::invalid_argument(
      std::string("reader number must be within the inclusive interval: [1, ") + std::to_string(MAX_READER_NUM) + ']'
  );
}

[[nodiscard]] constexpr auto power_of_two(uint8_t exponent) -> uint64_t {
  uint64_t result = 1;
  for (size_t i = 0; i < exponent; ++i) {
    result *= 2;
  }
  return result;
}

[[nodiscard]] constexpr auto log_base_two(uint64_t value) -> uint8_t {
  uint8_t result = 0;
  uint64_t n = value;
  while (n > 1) {
    n /= 2;
    result += 1;
  }
  return result;
}

// index: 0 corresponds to Reader: 1.
[[nodiscard]] constexpr auto create_binary_masks() -> std::array<uint64_t, MAX_READER_NUM> {
  std::array<uint64_t, MAX_READER_NUM> result{};
  uint8_t index = 0;
  for (auto& x : result) {
    x = power_of_two(index);
    index += 1;
  }
  return result;
}

constexpr std::array<uint64_t, MAX_READER_NUM> BINARY_MASKS = create_binary_masks();

class ReaderId {
 public:
  [[nodiscard]] static auto create(uint8_t num) noexcept(false) -> ReaderId {
    validate_reader_number(num);
    return ReaderId{num};
  }

  [[nodiscard]] static auto all_readers_mask(uint8_t total_reader_num) noexcept(false) -> uint64_t {
    validate_reader_number(total_reader_num);
    const uint64_t mask = power_of_two(total_reader_num) - 1L;
    return mask;
  }

  [[nodiscard]] auto value() const noexcept -> uint8_t { return this->value_; }

  [[nodiscard]] auto mask() const noexcept -> uint64_t {
    // see validate_reader_number, it guarantees that value_ is always within the bounds
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    return BINARY_MASKS[this->value_ - 1];
  }

  friend auto operator<<(std::ostream& os, const ReaderId& x) -> std::ostream&;

 private:
  explicit ReaderId(uint8_t value) noexcept : value_{value} {}

  uint8_t value_;
};

}  // namespace lshl::demux::core

#endif  // DEMUX_CPP_LSHL_DEMUX_READER_ID_H