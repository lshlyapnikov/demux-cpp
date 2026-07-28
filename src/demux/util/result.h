// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <variant>

namespace lshl::demux::util {

using Unit = std::monostate;

template <typename E = std::string, typename R = Unit>
class Result : public std::variant<E, R> {
 public:
  [[nodiscard]] constexpr auto is_error() const noexcept -> bool { return this->index() == 0; }

  [[nodiscard]] constexpr auto error() const& -> const E& { return std::get<0>(*this); }

  [[nodiscard]] constexpr auto is_value() const noexcept -> bool { return this->index() == 1; }

  [[nodiscard]] constexpr auto value() const& -> const R& { return std::get<1>(*this); }
};

template <typename E = std::string, typename R = Unit>
constexpr auto value(R result) -> Result<E, R> {
  return {result};
}

template <typename E = std::string, typename R = Unit>
constexpr auto error(const std::string& error_msg) -> Result<E, R> {
  return {error_msg};
}

using EmptyResult = Result<>;

inline constexpr EmptyResult empty_value = {Unit{}};

inline constexpr Result<std::string, bool> true_value = {true};
inline constexpr Result<std::string, bool> false_value = {false};

}  // namespace lshl::demux::util