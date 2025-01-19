// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner)

#define UNIT_TEST

#include "../core/message_buffer.h"
#include <gtest/gtest.h>
#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <tuple>
#include <vector>

using lshl::demux::core::MessageBuffer;
using std::array;
using std::span;
using std::uint8_t;
using std::vector;

auto create_test_array(const size_t size) -> vector<uint8_t> {
  vector<uint8_t> result{};
  result.resize(size);
  for (uint8_t i = 0; i < size; i++) {
    result[i] = i;
  }
  return result;
}

TEST(MessageBufferTest, MessageBufferRemaining0) {
  rc::check([](const uint8_t position) {
    std::cout << "position: " << static_cast<int>(position) << '\n';

    constexpr size_t BUF_SIZE = 32;

    std::array<uint8_t, BUF_SIZE> data{0};
    const MessageBuffer<BUF_SIZE> buf(span<uint8_t, BUF_SIZE>{data});

    const size_t actual = buf.remaining(position + BUF_SIZE);
    ASSERT_EQ(actual, 0);
  });
}

TEST(MessageBufferTest, MessageBufferRemaining) {
  rc::check([](const uint8_t position) {
    std::cout << "position: " << static_cast<int>(position) << '\n';

    constexpr size_t BUF_SIZE = 32;

    std::array<uint8_t, BUF_SIZE> data{0};
    const MessageBuffer<BUF_SIZE> buf(span<uint8_t, BUF_SIZE>{data});

    const size_t actual = buf.remaining(position);
    if (position >= BUF_SIZE) {
      RC_TAG("position >= BUF_SIZE");
      ASSERT_EQ(actual, 0);
    } else {
      RC_TAG("position < BUF_SIZE");
      ASSERT_EQ(actual, (BUF_SIZE - position));
    }
  });
}

TEST(MessageBufferTest, WriteToSharedMemory) {
  rc::check([](vector<uint8_t> message) {
    constexpr size_t BUF_SIZE = 32;

    std::array<uint8_t, BUF_SIZE> data{0};
    MessageBuffer<BUF_SIZE> buf(span<uint8_t, BUF_SIZE>{data});

    const size_t written = buf.write(0, message);
    if (message.size() + 2 <= BUF_SIZE) {
      ASSERT_EQ(written, message.size() + 2);

      uint16_t written_length = 0;
      std::copy_n(data.data(), sizeof(uint16_t), &written_length);
      ASSERT_EQ(written_length, message.size());

      const span<uint8_t> read = buf.read(0);
      ASSERT_EQ(read.size(), message.size());

      for (size_t i = 0; i < message.size(); ++i) {
        ASSERT_EQ(data.at(i + 2), message[i]);
        ASSERT_EQ(read[i], message[i]);
      }
    } else {
      ASSERT_EQ(written, 0);
    }
  });
}

TEST(MessageBufferTest, MessageBufferWriteAndRead) {
  rc::check([](const uint8_t position, const uint8_t src_size) {
    std::cout << "position: " << static_cast<int>(position) << ", src_size: " << static_cast<int>(src_size) << '\n';

    constexpr size_t BUF_SIZE = 32;

    std::array<uint8_t, BUF_SIZE> data{};
    MessageBuffer<BUF_SIZE> buf(data);

    const size_t remaining = buf.remaining(position);
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    vector<uint8_t> src = create_test_array(src_size);
    const size_t written = buf.write(position, src);
    if (remaining >= src_size + sizeof(uint16_t)) {
      ASSERT_EQ(src_size + sizeof(uint16_t), written);
      const span<uint8_t> read = buf.read(position);
      ASSERT_EQ(read.size(), src_size);
      // compare read bytes with the src bytes
      for (uint8_t i = 0; i < src_size; i++) {
        std::cout << "\ti:" << static_cast<int>(i) << ", value: " << static_cast<int>(src[i]) << '\n';
        ASSERT_EQ(read[i], src[i]);
      }
    } else {
      ASSERT_EQ(0, written);
    }
  });
}

TEST(MessageBufferTest, MessageBufferWriteEmpty) {
  constexpr size_t BUF_SIZE = 32;

  std::array<uint8_t, BUF_SIZE> data{};
  MessageBuffer<BUF_SIZE> buf(data);

  const span<uint8_t> empty_message{};
  const size_t written = buf.write(0, empty_message);

  ASSERT_EQ(written, sizeof(uint16_t));
  ASSERT_EQ(written, 2);
  const span<uint8_t> read = buf.read(0);
  ASSERT_EQ(read.size(), 0);
}

TEST(MessageBufferTest, MessageBufferAllocateAndRead) {
  rc::check([](const uint8_t position, const int32_t a0, const uint64_t a1, const double a2) {
    std::cout << "position: " << static_cast<int>(position) << ", a0: " << a0 << ", a1: " << a1 << ", a2: " << a2
              << '\n';

    using Tuple2 = std::tuple<int32_t, uint64_t>;
    using Tuple3 = std::tuple<int32_t, uint64_t, double>;

    constexpr size_t t2_needed = MessageBuffer<0>::required<Tuple2>();
    constexpr size_t t3_needed = MessageBuffer<0>::required<Tuple3>();
    constexpr size_t BUF_SIZE = (t2_needed + t3_needed) * 2;

    std::array<uint8_t, BUF_SIZE> data{};
    MessageBuffer<BUF_SIZE> buf(data);

    // allocate Tuple2 and set fields

    const size_t p2 = position;
    std::optional<Tuple2*> t2_opt = buf.allocate<Tuple2>(p2);

    if (buf.remaining(p2) >= t2_needed) {
      ASSERT_TRUE(t2_opt.has_value());
      Tuple2* t2 = t2_opt.value();
      std::get<0>(*t2) = a0;
      std::get<1>(*t2) = a1;
    } else {
      ASSERT_FALSE(t2_opt.has_value());
    }

    // allocate Tuple3 and set fields

    const size_t p3 = p2 + t2_needed;
    std::optional<Tuple3*> t3_opt = buf.allocate<Tuple3>(p3);

    if (buf.remaining(p3) >= t3_needed) {
      ASSERT_TRUE(t3_opt.has_value());
      Tuple3* t3 = t3_opt.value();
      std::get<0>(*t3) = a0;
      std::get<1>(*t3) = a1;
      std::get<2>(*t3) = a2;
    } else {
      ASSERT_FALSE(t3_opt.has_value());
    }

    // read Tuple2

    if (t2_opt.has_value()) {
      const std::span<uint8_t> x = buf.read(p2);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-type-const-cast, modernize-use-auto)
      const auto* const t2 = reinterpret_cast<const Tuple2*>(x.data());
      ASSERT_EQ(a0, std::get<0>(*t2));
      ASSERT_EQ(a1, std::get<1>(*t2));
    }

    // read Tuple3

    if (t3_opt.has_value()) {
      const std::span<uint8_t> x = buf.read(p3);
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-type-const-cast, modernize-use-auto)
      const auto* const t3 = reinterpret_cast<const Tuple3*>(x.data());
      ASSERT_EQ(a0, std::get<0>(*t3));
      ASSERT_EQ(a1, std::get<1>(*t3));
      ASSERT_EQ(a2, std::get<2>(*t3));
    }
  });
}

// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)