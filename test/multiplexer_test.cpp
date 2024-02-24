#include "../src/multiplexer.h"
#include <gtest/gtest.h>
#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

using ShmSequencer::MultiplexerPublisher;
using std::uint16_t;
using std::uint8_t;
using std::vector;

// namespace rc {
// template <>
// struct Arbitrary<span<uint8_t>> {
//   static const size_t MAX_SIZE = 32;
//   static Gen<span<uint8_t>> arbitrary() {
//     return {
//       const size_t n = *rc::gen::inRange(0, MAX_SIZE);

//       gen::build<span<uint8_t>>(
//       //     gen::set(&Person::firstName), gen::set(&Person::lastName), gen::set(&Person::age, gen::inRange(0,
//       100)));
//     }
//   }
// };
// }  // namespace rc

// TEST(MultiplexerTest, Packet) {
//   const size_t M = 8;
//   Packet<M> packet;

//   const std::array<uint8_t, M> src = {1, 2, 3, 4, 5, 6, 7, 8};
//   packet.read_from(src, 3);
//   ASSERT_EQ(3, packet.size());

//   std::array<uint8_t, M> dst{0, 0, 0, 0, 0, 0, 0, 0};
//   const uint16_t result = packet.write_into(&dst);
//   ASSERT_EQ(result, 3);

//   const std::array<uint8_t, M> expected = {1, 2, 3, 0, 0, 0, 0, 0};
//   EXPECT_EQ(dst, expected);
// }

// NOLINTBEGIN(misc-include-cleaner,cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
// TEST(MultiplexerTest, RoundtripReadWrite) {
//   rc::check("Packet::read_from followed by Packet::write_into does not not lose any data",
//             [](const std::array<uint8_t, 256>& a0, const uint8_t size) {
//               Packet<256> packet;
//               packet.read_from(a0, size);

//               std::array<uint8_t, 256> a1{};
//               const uint16_t written_bytes = packet.write_into(&a1);

//               const auto data0 = vector<uint8_t>(std::begin(a0), std::begin(a0) + size);
//               const auto data1 = vector<uint8_t>(std::begin(a1), std::begin(a1) + size);

//               ASSERT_TRUE(written_bytes == size);
//               ASSERT_TRUE(data0 == data1);
//             });
// }
// NOLINTEND(misc-include-cleaner,cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

TEST(MultiplexerTest, Constructor) {
  MultiplexerPublisher<32, 4> m(5);
  MultiplexerPublisher<32, 4> m1(32);

  try {
    MultiplexerPublisher<32, 4> m3(0);
    FAIL() << "Expected exception here";
  } catch (std::invalid_argument e) {
    ASSERT_STREQ(e.what(), "subscriber_number must be within the inclusive interval: [1, 32]");
  }

  try {
    MultiplexerPublisher<32, 4> m3(33);
    FAIL() << "Expected exception here";
  } catch (std::invalid_argument e) {
    ASSERT_STREQ(e.what(), "subscriber_number must be within the inclusive interval: [1, 32]");
  }
}
