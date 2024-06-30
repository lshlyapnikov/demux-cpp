// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#ifndef DEMUX_CPP_LSHL_DEMUX_SUBSCRIBER_ID_H
#define DEMUX_CPP_LSHL_DEMUX_SUBSCRIBER_ID_H

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace lshl::demux {

using std::size_t;
using std::uint16_t;
using std::uint64_t;
using std::uint8_t;

// template <class T>
// class NewType {
//  public:
//   NewType(T value) : value_(std::move(value)) {}

//   [[nodiscard]] auto value() const noexcept -> & T { return this->value_; }

//  private:
//   T value_;
// };

constexpr uint8_t MAX_SUBSCRIBER_NUM = sizeof(uint64_t) * 8;  // 64

[[nodiscard]] constexpr auto is_valid_subscriber_number(uint8_t num) noexcept -> bool {
  return 1 <= num && num <= MAX_SUBSCRIBER_NUM;
}

constexpr auto validate_subscriber_number(uint8_t num) noexcept(false) -> uint8_t {
  if (is_valid_subscriber_number(num)) {
    return num;
  }
  throw std::invalid_argument(std::string("subscriber_number must be within the inclusive interval: [1, ") +
                              std::to_string(MAX_SUBSCRIBER_NUM) + ']');
}

auto my_pow(uint8_t x, uint8_t p) -> uint64_t;

class SubscriberId {
 public:
  [[nodiscard]] static auto create(uint8_t num) noexcept(false) -> SubscriberId {
    validate_subscriber_number(num);
    const uint8_t index = num - 1;
    const uint64_t mask = my_pow(2, index);
    return SubscriberId{mask, index};
  }

  [[nodiscard]] static auto all_subscribers_mask(uint8_t total_subscriber_num) noexcept(false) -> uint64_t {
    validate_subscriber_number(total_subscriber_num);
    const uint64_t mask = my_pow(2, total_subscriber_num) - 1L;
    return mask;
  }

  [[nodiscard]] auto mask() const noexcept -> uint64_t { return this->mask_; }
  [[nodiscard]] auto index() const noexcept -> size_t { return this->index_; }

  friend auto operator<<(std::ostream& os, const SubscriberId& sub) -> std::ostream&;

 private:
  SubscriberId(uint64_t mask, size_t index) noexcept : mask_(mask), index_(index) {}

  uint64_t mask_;
  size_t index_;
};

}  // namespace lshl::demux

#endif  // DEMUX_CPP_LSHL_DEMUX_SUBSCRIBER_ID_H