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
#include "../src/domain.h"

using ShmSequencer::MultiplexerPublisher;
using ShmSequencer::SubscriberId;
using std::uint16_t;
using std::uint8_t;
using std::vector;

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

TEST(DomainTest, Constants) {
  EXPECT_EQ(ShmSequencer::MAX_SUBSCRIBER_NUM, 32);
}

TEST(DomainTest, SubscriberIdManualCheck) {
  for (uint8_t num : {0, 33, 128, 255}) {
    ASSERT_THROW({ std::ignore = SubscriberId::create(num); }, std::invalid_argument);
    ASSERT_THROW({ std::ignore = SubscriberId::all_subscribers_mask(num); }, std::invalid_argument);
  }
  {
    const SubscriberId sub_id = SubscriberId::create(1);
    ASSERT_EQ(0, sub_id.index());
    ASSERT_EQ(0b1, sub_id.mask());

    const uint32_t mask = SubscriberId::all_subscribers_mask(1);
    ASSERT_EQ(0b1, mask);
  }
  {
    const SubscriberId sub_id = SubscriberId::create(2);
    ASSERT_EQ(1, sub_id.index());
    ASSERT_EQ(0b10, sub_id.mask());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(2);
    ASSERT_EQ(0b11, mask.value());
  }
  {
    const SubscriberId sub_id = SubscriberId::create(3);
    ASSERT_EQ(2, sub_id.index());
    ASSERT_EQ(0b100, sub_id.mask());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(3);
    ASSERT_EQ(0b111, mask.value());
  }
  {
    const SubscriberId sub_id = SubscriberId::create(4);
    ASSERT_EQ(3, sub_id.index());
    ASSERT_EQ(0b1000, sub_id.mask());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(4);
    ASSERT_EQ(0b1111, mask.value());
  }
  {
    const SubscriberId sub_id = SubscriberId::create(31);
    ASSERT_EQ(30, sub_id.index());
    ASSERT_EQ(0b1000000000000000000000000000000, sub_id.mask());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(31);
    ASSERT_EQ(0b1111111111111111111111111111111, mask.value());
  }
  {
    const SubscriberId sub_id = SubscriberId::create(32);
    ASSERT_EQ(31, sub_id.index());
    ASSERT_EQ(0b10000000000000000000000000000000, sub_id.mask());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(32);
    ASSERT_EQ(0b11111111111111111111111111111111, mask.value());
  }
}

TEST(DomainTest, SubscriberId) {
  rc::check("SubscriberId::create", [](const uint8_t num) {
    if (num > ShmSequencer::MAX_SUBSCRIBER_NUM || num == 0) {
      ASSERT_THROW({ std::ignore = SubscriberId::create(num); }, std::invalid_argument);
    } else {
      const SubscriberId sub_id = SubscriberId::create(num);
      ASSERT_EQ(num - 1, sub_id.index());
      ASSERT_EQ(pow(2, (num - 1)), sub_id.mask());
    }
  });
}

TEST(DomainTest, AllSubscribersMask) {
  rc::check("SubscriberId::all_subscribers_mask", [](const uint8_t num) {
    if (num > ShmSequencer::MAX_SUBSCRIBER_NUM || num == 0) {
      ASSERT_THROW({ std::ignore = SubscriberId::all_subscribers_mask(num); }, std::invalid_argument);
    } else {
      const uint32_t all_mask = SubscriberId::all_subscribers_mask(num);
      ASSERT_EQ(pow(2, num) - 1, all_mask);
    }
  });
}

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
