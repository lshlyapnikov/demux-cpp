// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(misc-include-cleaner)

#ifndef DEMUX_CPP_LSHL_DEMUX_ENDPOINT_ID_TEST_H
#define DEMUX_CPP_LSHL_DEMUX_ENDPOINT_ID_TEST_H

#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <cstdint>
#include "../core/endpoint_id.h"

namespace rc {

template <>
struct Arbitrary<lshl::demux::core::EndpointId> {
  static auto arbitrary() -> Gen<lshl::demux::core::EndpointId> {
    using lshl::demux::core::EndpointId;
    using lshl::demux::core::is_valid_endpoint_id;
    using lshl::demux::core::MAX_ENDPOINT_NUM;

    const Gen<uint8_t> range = gen::inRange(static_cast<uint8_t>(1), MAX_ENDPOINT_NUM);
    const Gen<uint8_t> filtered = gen::suchThat(range, is_valid_endpoint_id);
    return gen::map(filtered, [](uint8_t x) { return EndpointId(x); });
  }
};

}  // namespace rc

#endif  // DEMUX_CPP_LSHL_DEMUX_ENDPOINT_ID_TEST_H

// NOLINTEND(misc-include-cleaner)