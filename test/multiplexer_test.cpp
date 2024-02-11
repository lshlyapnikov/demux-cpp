#include "../src/multiplexer.h"
#include <gtest/gtest.h>
#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>
#include "../src/domain.h"

using ShmSequencer::MultiplexerPublisher;
using ShmSequencer::Packet;
using ShmSequencer::SubscriberId;
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

TEST(DomainTest, Constants) {
  EXPECT_EQ(ShmSequencer::MAX_SUBSCRIBER_NUM, 32);
}

TEST(DomainTest, SubscriberIdManualCheck) {
  {
    const std::optional<SubscriberId> actual = SubscriberId::create(0);
    ASSERT_FALSE(actual.has_value());
  }
  {
    const std::optional<SubscriberId> actual = SubscriberId::create(33);
    ASSERT_FALSE(actual.has_value());
  }
  {
    const std::optional<SubscriberId> actual = SubscriberId::create(255);
    ASSERT_FALSE(actual.has_value());
  }
  {
    const std::optional<SubscriberId> actual = SubscriberId::create(1);
    ASSERT_TRUE(actual.has_value());
    ASSERT_EQ(0, actual.value().index());
    ASSERT_EQ(1, actual.value().mask());
  }
  {
    const std::optional<SubscriberId> actual = SubscriberId::create(2);
    ASSERT_TRUE(actual.has_value());
    ASSERT_EQ(1, actual.value().index());
    ASSERT_EQ(0b10, actual.value().mask());
  }
  {
    const std::optional<SubscriberId> actual = SubscriberId::create(3);
    ASSERT_TRUE(actual.has_value());
    ASSERT_EQ(2, actual.value().index());
    ASSERT_EQ(0b100, actual.value().mask());
  }
  {
    const std::optional<SubscriberId> actual = SubscriberId::create(4);
    ASSERT_TRUE(actual.has_value());
    ASSERT_EQ(3, actual.value().index());
    ASSERT_EQ(0b1000, actual.value().mask());
  }
  {
    const std::optional<SubscriberId> actual = SubscriberId::create(31);
    ASSERT_TRUE(actual.has_value());
    ASSERT_EQ(30, actual.value().index());
    ASSERT_EQ(0b1000000000000000000000000000000, actual.value().mask());
  }
  {
    const std::optional<SubscriberId> actual = SubscriberId::create(32);
    ASSERT_TRUE(actual.has_value());
    ASSERT_EQ(31, actual.value().index());
    ASSERT_EQ(0b10000000000000000000000000000000, actual.value().mask());
  }
}

TEST(DomainTest, SubscriberId) {
  rc::check("SubscriberId::create", [](const uint8_t id) {
    // std::cout << "id: " << static_cast<uint32_t>(id) << std::endl;
    const std::optional<SubscriberId> actual = SubscriberId::create(id);
    if (id > ShmSequencer::MAX_SUBSCRIBER_NUM || id == 0) {
      ASSERT_FALSE(actual.has_value());
    } else {
      ASSERT_TRUE(actual.has_value());
      ASSERT_EQ(id - 1, actual.value().index());
      ASSERT_EQ(pow(2, (id - 1)), actual.value().mask());
    }
  });
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

              ASSERT_TRUE(written_bytes == size);
              ASSERT_TRUE(data0 == data1);
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
    ASSERT_EQ(l0, l1);
  });
}
// NOLINTEND(misc-include-cleaner,cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)