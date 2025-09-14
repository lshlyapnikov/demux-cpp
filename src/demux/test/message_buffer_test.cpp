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
#include "../util/shm_util.h"
#include "../util/tuple_util.h"

using lshl::demux::core::MessageBuffer;
using std::array;
using std::span;
using std::uint8_t;
using std::vector;

using Tuple1 = std::tuple<int16_t>;
using Tuple2 = std::tuple<int32_t, uint64_t>;
using Tuple3 = std::tuple<int32_t, uint64_t, double>;

namespace {
auto create_test_array(const size_t size) -> vector<uint8_t> {
  vector<uint8_t> result{};
  result.resize(size);
  for (std::size_t i = 0; i < size; i++) {
    result[i] = static_cast<uint8_t>(i);
  }
  return result;
}
}  // namespace

TEST(MessageBufferTest, MessageBufferRemaining0) {
  rc::check([](const uint8_t position) {
    std::cout << "position: " << static_cast<int>(position) << '\n';

    constexpr size_t BUF_SIZE = 32;

    std::array<uint8_t, BUF_SIZE> data{0};
    const MessageBuffer<BUF_SIZE> buf(span<uint8_t, BUF_SIZE>{data});

    const size_t actual = buf.remaining(position + BUF_SIZE);
    EXPECT_EQ(actual, 0);

    return !::testing::Test::HasFailure();
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
      EXPECT_EQ(actual, 0);
    } else {
      RC_TAG("position < BUF_SIZE");
      EXPECT_EQ(actual, (BUF_SIZE - position));
    }

    return !::testing::Test::HasFailure();
  });
}

TEST(MessageBufferTest, WriteToSharedMemory) {
  rc::check([](vector<uint8_t> message) {
    constexpr size_t BUF_SIZE = 32;

    std::array<uint8_t, BUF_SIZE> data{0};
    MessageBuffer<BUF_SIZE> buf(span<uint8_t, BUF_SIZE>{data});

    using lshl::demux::util::operator<<;
    std::cout << "data:    " << data << '\n';

    // write message to the buffer
    const size_t written = buf.write(0, message);

    if (message.size() + 2 <= BUF_SIZE) {
      EXPECT_EQ(written, message.size() + 2);

      // copy 2 byte length field and check it
      uint16_t length = 0;
      std::copy_n(data.data(), sizeof(uint16_t), &length);
      EXPECT_EQ(length, message.size());

      // read message from the buffer
      const span<uint8_t> read = buf.read(0);
      EXPECT_EQ(read.size(), message.size());

      using lshl::demux::util::operator<<;
      std::cout << "message: " << message << '\n';
      std::cout << "data:    " << data << '\n';
      std::cout << "buffer:  " << buf.data() << '\n';
      std::cout << "read:    " << read << '\n';
      std::cout << '\n';

      // compare original message with buffer and read message
      for (size_t i = 0; i < message.size(); ++i) {
        EXPECT_EQ(data.at(i + 2), message[i]);
        EXPECT_EQ(read[i], message[i]);
      }
    } else {
      EXPECT_EQ(written, 0);
    }

    return !::testing::Test::HasFailure();
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
      EXPECT_EQ(src_size + sizeof(uint16_t), written);
      const span<uint8_t> read = buf.read(position);
      EXPECT_EQ(read.size(), src_size);
      // compare read bytes with the src bytes
      for (uint8_t i = 0; i < src_size; i++) {
        std::cout << "\ti:" << static_cast<int>(i) << ", value: " << static_cast<int>(src[i]) << '\n';
        EXPECT_EQ(read[i], src[i]);
        if (::testing::Test::HasFailure()) {
          return false;
        }
      }
    } else {
      EXPECT_EQ(0, written);
    }

    return !::testing::Test::HasFailure();
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
  rc::check([](size_t position, const Tuple1& x1, const Tuple2& x2, const Tuple3& x3) {
    using lshl::demux::util::operator<<;

    constexpr size_t t1_needed = MessageBuffer<0>::required<Tuple1>();
    constexpr size_t t2_needed = MessageBuffer<0>::required<Tuple2>();
    constexpr size_t t3_needed = MessageBuffer<0>::required<Tuple3>();

    constexpr size_t BUF_SIZE = (t1_needed + t2_needed + t3_needed) * 2;

    position = position % BUF_SIZE;

    std::cout << "position: " << static_cast<int>(position) << ", x1: " << x1 << ", x2: " << x2 << ", x3: " << x3
              << ", BUF_SIZE: " << BUF_SIZE << '\n';

    std::array<uint8_t, BUF_SIZE> data{};
    MessageBuffer<BUF_SIZE> buf(data);

    // allocate Tuple1 and set fields

    const size_t p1 = position;
    std::optional<Tuple1*> t1_opt = buf.allocate<Tuple1>(p1);

    if (buf.remaining(p1) >= t1_needed) {
      EXPECT_TRUE(t1_opt.has_value());
      Tuple1* t1 = t1_opt.value();
      *t1 = x1;
    } else {
      EXPECT_FALSE(t1_opt.has_value());
    }

    // allocate Tuple2 and set fields

    const size_t p2 = p1 + t1_needed;
    std::optional<Tuple2*> t2_opt = buf.allocate<Tuple2>(p2);

    if (buf.remaining(p2) >= t2_needed) {
      EXPECT_TRUE(t2_opt.has_value());
      Tuple2* t2 = t2_opt.value();
      *t2 = x2;
    } else {
      EXPECT_FALSE(t2_opt.has_value());
    }

    // allocate Tuple3 and set fields

    const size_t p3 = p2 + t2_needed;
    std::optional<Tuple3*> t3_opt = buf.allocate<Tuple3>(p3);

    if (buf.remaining(p3) >= t3_needed) {
      EXPECT_TRUE(t3_opt.has_value());
      Tuple3* t3 = t3_opt.value();
      *t3 = x3;
    } else {
      EXPECT_FALSE(t3_opt.has_value());
    }

    // read Tuple1
    if (t1_opt.has_value()) {
      RC_TAG("Tuple1");
      const std::optional<const Tuple1*> read = buf.read_unsafe<Tuple1>(p1);
      EXPECT_TRUE(read.has_value());
      EXPECT_EQ(x1, *(read.value()));
    }

    // read Tuple2
    if (t2_opt.has_value()) {
      RC_TAG("Tuple2");
      const std::optional<const Tuple2*> read = buf.read_unsafe<Tuple2>(p2);
      EXPECT_TRUE(read.has_value());
      EXPECT_EQ(x2, *(read.value()));
    }

    // read Tuple3
    if (t3_opt.has_value()) {
      RC_TAG("Tuple3");
      const std::optional<const Tuple3*> read = buf.read_unsafe<Tuple3>(p3);
      EXPECT_TRUE(read.has_value());
      EXPECT_EQ(x3, *(read.value()));
    }

    // read Tuple3 at non-existent position
    {
      RC_TAG("Non-existent");
      const std::optional<const Tuple3*> read = buf.read_unsafe<Tuple3>(BUF_SIZE);
      EXPECT_FALSE(read.has_value());
    }

    return !::testing::Test::HasFailure();
  });
}

// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)