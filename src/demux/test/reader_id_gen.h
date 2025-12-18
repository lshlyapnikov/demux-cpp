// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <cstdint>
#include "../core/reader_id.h"

namespace rc {

template <>
struct Arbitrary<lshl::demux::core::ReaderId> {
  static auto arbitrary() -> Gen<lshl::demux::core::ReaderId> {
    using lshl::demux::core::ReaderId;

    const Gen<uint8_t> range = gen::inRange(static_cast<uint8_t>(1), static_cast<uint8_t>(32));
    return gen::map(range, [](uint8_t x) { return ReaderId(x); });
  }
};

}  // namespace rc
