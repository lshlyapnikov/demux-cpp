#pragma once

#include <boost/serialization/strong_typedef.hpp>
#include <cstdint>
#include <iostream>
#include <vector>

namespace lshl::demux::core::test {

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
BOOST_STRONG_TYPEDEF(std::vector<std::uint8_t>, TestMessage);

auto operator<<(std::ostream& os, const TestMessage& message) -> std::ostream&;

}  // namespace lshl::demux::core::test