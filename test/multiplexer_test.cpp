#include "../src/multiplexer.h"
#include <gtest/gtest.h>
#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

using ShmSequencer::Packet;
using std::uint16_t;
using std::uint8_t;
using std::vector;

TEST(MultiplexerTest, Packet) {
  const size_t M = 8;
  Packet<M> packet;

  const std::array<uint8_t, M> src = {1, 2, 3, 4, 5, 6, 7, 8};
  packet.read_from(src, 3);
  ASSERT_EQ(3, packet.size());

  std::array<uint8_t, M> dst{0, 0, 0, 0, 0, 0, 0, 0};
  const uint16_t result = packet.write_into(&dst);
  ASSERT_EQ(result, 3);

  const std::array<uint8_t, M> expected = {1, 2, 3, 0, 0, 0, 0, 0};
  EXPECT_EQ(dst, expected);
}

// NOLINTBEGIN(misc-include-cleaner,cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
TEST(MultiplexerTest, RoundtripReadWrite) {
  rc::check("Packet::read_from followed by Packet::write_into does not not lose any data",
            [](const std::array<uint8_t, 256>& a0, const uint8_t size) {
              Packet<256> packet;
              packet.read_from(a0, size);

              std::array<uint8_t, 256> a1{};
              const uint16_t written_bytes = packet.write_into(&a1);

              const auto data0 = vector<uint8_t>(std::begin(a0), std::begin(a0) + size);
              const auto data1 = vector<uint8_t>(std::begin(a1), std::begin(a1) + size);

              RC_ASSERT((written_bytes == size) && (data0 == data1));
            });
}
// NOLINTEND(misc-include-cleaner,cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

// NOLINTBEGIN(misc-include-cleaner,cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
TEST(MultiplexerTest, Rapidcheck) {
  rc::check("double reversal yields the original value", [](const std::array<int, 8>& l0) {
    auto l1 = l0;

    // for (auto x : l0) {
    //   std::cout << x << ',';
    // }
    // std::cout << '\n';

    std::reverse(begin(l1), end(l1));
    std::reverse(begin(l1), end(l1));
    RC_ASSERT(l0 == l1);
  });
}
// NOLINTEND(misc-include-cleaner,cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)