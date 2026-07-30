// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace lshl::demux::core {

using std::size_t;
using std::uint16_t;
using std::uint64_t;
using std::uint8_t;

class ReaderId {
 private:
  uint8_t value_;

 public:
  ReaderId() noexcept : value_{0} {}
  explicit ReaderId(uint8_t id) noexcept(false) : value_{id} {}

  [[nodiscard]] auto value() const noexcept -> uint8_t { return this->value_; }

  auto operator==(const ReaderId& x) const noexcept -> bool { return this->value_ == x.value_; }

  auto operator<(const ReaderId& x) const noexcept -> bool { return this->value_ < x.value_; }

  friend auto operator<<(std::ostream& os, const ReaderId& x) -> std::ostream&;

  friend auto operator<<(std::ostream& os, const std::vector<ReaderId>& xs) -> std::ostream&;
};

}  // namespace lshl::demux::core
