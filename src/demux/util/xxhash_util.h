// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#ifndef LSHL_UTIL_XXHASH_UTIL_H
#define LSHL_UTIL_XXHASH_UTIL_H

#include <xxhash.h>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace lshl::util {

struct XXH64_util {
  explicit XXH64_util() noexcept(false) : state_(XXH64_createState()) {
    if (nullptr == this->state_) {
      throw std::domain_error("XXH64_createState failed");
    }
    if (XXH64_reset(this->state_, 0) == XXH_ERROR) {
      throw std::domain_error("XXH64_reset failed");
    }
  };

  ~XXH64_util() { XXH64_freeState(this->state_); }

  XXH64_util(const XXH64_util&) = delete;                         // copy constructor
  auto operator=(const XXH64_util&) -> XXH64_util& = delete;      // copy assignment
  XXH64_util(XXH64_util&&) noexcept = delete;                     // move constructor
  auto operator=(XXH64_util&&) noexcept -> XXH64_util& = delete;  // move assignment

  auto update(const void* input, std::size_t size) noexcept(false) {
    if (XXH64_update(this->state_, input, size) == XXH_ERROR) {
      throw std::domain_error("XXH64_update failed");
    }
  }

  auto digest() -> XXH64_hash_t { return XXH64_digest(this->state_); }

  static auto format(XXH64_hash_t digest) -> std::string {
    constexpr std::size_t INT64_HEX_MAX_CHAR_LEN = 16;
    std::stringstream result;
    result << std::hex << std::setw(INT64_HEX_MAX_CHAR_LEN) << std::setfill('0') << digest << std::dec;
    return result.str();
  }

 private:
  XXH64_state_t* state_;
};

}  // namespace lshl::util

#endif  // LSHL_UTIL_XXHASH_UTIL_H