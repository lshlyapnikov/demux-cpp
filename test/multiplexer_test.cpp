#include "../src/multiplexer.h"
#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <sstream>

using ShmSequencer::Packet;

TEST(MultiplexerTest, Packet) {
  const size_t M = 8;
  Packet<M> packet;

  std::array<uint8_t, M> src = {1, 2, 3, 4, 5, 6, 7, 8};
  packet.copy_from(src, 3);
  ASSERT_EQ(3, packet.size());

  std::array<uint8_t, M> dst{0, 0, 0, 0, 0, 0, 0, 0};
  uint16_t result = packet.copy_into(dst);
  ASSERT_EQ(result, 3);

  const std::array<uint8_t, M> expected = {1, 2, 3, 0, 0, 0, 0, 0};
  EXPECT_EQ(dst, expected);
}

TEST(MultiplexerTest, Rapidcheck) {
  rc::check("double reversal yields the original value", [](const std::array<int, 8>& l0) {
    auto l1 = l0;

    // for (auto x : l0) {
    //   std::cout << x << ',';
    // }
    // std::cout << std::endl;

    std::reverse(begin(l1), end(l1));
    std::reverse(begin(l1), end(l1));
    RC_ASSERT(l0 == l1);
  });
}