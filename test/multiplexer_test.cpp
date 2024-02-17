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
#include <vector>
#include "../src/domain.h"

using ShmSequencer::MultiplexerPublisher;
using ShmSequencer::Packet;
using ShmSequencer::SubscriberId;
using std::uint16_t;
using std::uint8_t;
using std::vector;

auto test_span_fn1(std::span<uint8_t> buf) -> void {
  std::cout << "1. buf[0]: " << (int)buf[0] << ", buf.size: " << buf.size() << " buf.size_bytes: " << buf.size_bytes()
            << std::endl;
}

auto test_span_fn2(std::span<uint8_t, 8> buf) -> void {
  std::cout << "2. buf[0]: " << (int)buf[0] << ", buf.size: " << buf.size() << " buf.size_bytes: " << buf.size_bytes()
            << std::endl;
}

template <typename T, std::size_t M>
auto test_span_fn3(std::span<T, M> buf) -> void {
  std::cout << "3. buf[0]: " << (int)buf[0] << ", buf.size: " << buf.size() << " buf.size_bytes: " << buf.size_bytes()
            << std::endl;
}

TEST(MultiplexerTest, Span) {
  std::array<uint8_t, 8> buf1{
      0,
      1,
      2,
      3,
      4,
      5,
      6,
      7,
  };

  test_span_fn1(buf1);
  test_span_fn2(buf1);
  // test_span_fn3(buf1);

  std::array<uint8_t, 3> buf2{0, 1, 2};

  test_span_fn1(buf2);
  // test_span_fn2(buf2);
  // test_span_fn3(buf2);

  std::array<uint16_t, 3> buf3{0, 1, 2};
  test_span_fn3(std::span(buf3));
}

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
    const std::optional<SubscriberId> sub_id = SubscriberId::create(0);
    ASSERT_FALSE(sub_id.has_value());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(0);
    ASSERT_FALSE(mask.has_value());
  }
  {
    const std::optional<SubscriberId> sub_id = SubscriberId::create(33);
    ASSERT_FALSE(sub_id.has_value());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(33);
    ASSERT_FALSE(mask.has_value());
  }
  {
    const std::optional<SubscriberId> sub_id = SubscriberId::create(255);
    ASSERT_FALSE(sub_id.has_value());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(255);
    ASSERT_FALSE(mask.has_value());
  }
  {
    const std::optional<SubscriberId> sub_id = SubscriberId::create(1);
    ASSERT_TRUE(sub_id.has_value());

    ASSERT_EQ(0, sub_id.value().index());
    ASSERT_EQ(0b1, sub_id.value().mask());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(1);
    ASSERT_TRUE(mask.has_value());
    ASSERT_EQ(0b1, mask.value());
  }
  {
    const std::optional<SubscriberId> sub_id = SubscriberId::create(2);
    ASSERT_TRUE(sub_id.has_value());
    ASSERT_EQ(1, sub_id.value().index());
    ASSERT_EQ(0b10, sub_id.value().mask());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(2);
    ASSERT_TRUE(mask.has_value());
    ASSERT_EQ(0b11, mask.value());
  }
  {
    const std::optional<SubscriberId> sub_id = SubscriberId::create(3);
    ASSERT_TRUE(sub_id.has_value());
    ASSERT_EQ(2, sub_id.value().index());
    ASSERT_EQ(0b100, sub_id.value().mask());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(3);
    ASSERT_TRUE(mask.has_value());
    ASSERT_EQ(0b111, mask.value());
  }
  {
    const std::optional<SubscriberId> sub_id = SubscriberId::create(4);
    ASSERT_TRUE(sub_id.has_value());
    ASSERT_EQ(3, sub_id.value().index());
    ASSERT_EQ(0b1000, sub_id.value().mask());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(4);
    ASSERT_TRUE(mask.has_value());
    ASSERT_EQ(0b1111, mask.value());
  }
  {
    const std::optional<SubscriberId> sub_id = SubscriberId::create(31);
    ASSERT_TRUE(sub_id.has_value());
    ASSERT_EQ(30, sub_id.value().index());
    ASSERT_EQ(0b1000000000000000000000000000000, sub_id.value().mask());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(31);
    ASSERT_TRUE(mask.has_value());
    ASSERT_EQ(0b1111111111111111111111111111111, mask.value());
  }
  {
    const std::optional<SubscriberId> sub_id = SubscriberId::create(32);
    ASSERT_TRUE(sub_id.has_value());
    ASSERT_EQ(31, sub_id.value().index());
    ASSERT_EQ(0b10000000000000000000000000000000, sub_id.value().mask());

    const std::optional<uint32_t> mask = SubscriberId::all_subscribers_mask(32);
    ASSERT_TRUE(mask.has_value());
    ASSERT_EQ(0b11111111111111111111111111111111, mask.value());
  }
}

TEST(DomainTest, SubscriberId) {
  rc::check("SubscriberId::create", [](const uint8_t num) {
    const std::optional<SubscriberId> sub_id = SubscriberId::create(num);
    const std::optional<uint32_t> all_mask = SubscriberId::all_subscribers_mask(num);
    if (num > ShmSequencer::MAX_SUBSCRIBER_NUM || num == 0) {
      ASSERT_FALSE(sub_id.has_value());
      ASSERT_FALSE(all_mask.has_value());
    } else {
      ASSERT_TRUE(sub_id.has_value());
      ASSERT_EQ(num - 1, sub_id.value().index());
      ASSERT_EQ(pow(2, (num - 1)), sub_id.value().mask());

      ASSERT_TRUE(all_mask.has_value());
      ASSERT_EQ(pow(2, num) - 1, all_mask.value());
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