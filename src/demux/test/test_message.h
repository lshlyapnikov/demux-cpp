#pragma once

#include <rapidcheck.h>
#include <boost/serialization/strong_typedef.hpp>
#include <cstdint>
#include <iostream>
#include <vector>

namespace lshl::demux::core::test {

constexpr size_t L = 128;
constexpr uint16_t M = 64;

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
BOOST_STRONG_TYPEDEF(std::vector<std::uint8_t>, TestMessage);

auto operator<<(std::ostream& os, const TestMessage& message) -> std::ostream&;

}  // namespace lshl::demux::core::test

namespace rc {

using lshl::demux::core::test::L;
using lshl::demux::core::test::M;
using lshl::demux::core::test::TestMessage;
using std::vector;

template <>
struct Arbitrary<TestMessage> {
  static auto arbitrary() -> Gen<TestMessage> {
    // looks like gen::resize also affects the generated uint8_t values, use the entire byte range [0, 255]
    // enable TEST(TestMessageGenerator, CheckByteDistribution)
    const Gen<vector<uint8_t>> nonempty_vec_with_improved_distribution_gen =
        gen::resize(std::numeric_limits<uint8_t>::max(), gen::nonEmpty<vector<uint8_t>>());
    const Gen<vector<uint8_t>> nonempty_vec_gen = gen::resize(M, nonempty_vec_with_improved_distribution_gen);
    // The above generator might still generate a vector with `size() > M`, `gen::resize` does not always work.
    // Run TestMessageGenerator.CheckDistribution1 500 times and check `RC_CLASSIFY(message_size > M, "size > M")`
    const Gen<vector<uint8_t>> valid_size_vec_gen = gen::suchThat(nonempty_vec_gen, is_valid_length);

    return gen::map(valid_size_vec_gen, [](const vector<uint8_t>& xs) { return TestMessage(xs); });
  }

  static auto is_valid_length(const vector<uint8_t>& xs) -> bool { return !xs.empty() && xs.size() <= M; }
};

}  // namespace rc