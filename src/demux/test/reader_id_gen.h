// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(misc-include-cleaner)

#pragma once

#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <cstdint>
#include "../core/reader_id.h"

namespace rc {

template <>
struct Arbitrary<lshl::demux::core::ReaderId> {
  static auto arbitrary() -> Gen<lshl::demux::core::ReaderId> {
    using lshl::demux::core::is_valid_reader_id;
    using lshl::demux::core::MAX_READER_NUM;
    using lshl::demux::core::ReaderId;

    const Gen<uint8_t> range = gen::inRange(static_cast<uint8_t>(1), MAX_READER_NUM);
    const Gen<uint8_t> filtered = gen::suchThat(range, is_valid_reader_id);
    return gen::map(filtered, [](uint8_t x) { return ReaderId(x); });
  }
};

}  // namespace rc

// NOLINTEND(misc-include-cleaner)