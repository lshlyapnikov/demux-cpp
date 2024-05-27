#include "../src/multiplexer.h"
#include <gtest/gtest.h>
#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

using ShmSequencer::MessageBuffer;
using ShmSequencer::MultiplexerPublisher;
using std::span;
using std::uint16_t;
using std::uint8_t;
using std::unique_ptr;

const uint16_t TEST_MAX_MSG_SIZE = 32;

auto create_test_array(const size_t size) -> unique_ptr<uint8_t[]> {
  unique_ptr<uint8_t[]> result(new uint8_t[size]);
  for (uint8_t i = 0; i < size; i++) {
    result.get()[i] = i;
  }
  return result;
}

TEST(MultiplexerTest, Atomic) {
  ASSERT_EQ(std::atomic<uint8_t>{}.is_lock_free(), true);
  ASSERT_EQ(std::atomic<uint16_t>{}.is_lock_free(), true);
  ASSERT_EQ(std::atomic<uint32_t>{}.is_lock_free(), true);
  ASSERT_EQ(std::atomic<size_t>{}.is_lock_free(), true);
  ASSERT_EQ(std::atomic<uint64_t>{}.is_lock_free(), true);
  ASSERT_EQ(sizeof(size_t), sizeof(uint64_t));
}

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

// TEST(MultiplexerTest, MessageBuffer_constructor) {
//   rc::check("MessageBuffer::constructor", [](const uint8_t size) {
//     if (size >= TEST_MAX_MSG_SIZE + sizeof(uint16_t)) {
//       MessageBuffer buf(size);
//       ASSERT_EQ(size, buf.remaining(0));
//     } else {
//       ASSERT_THROW({ std::ignore = MessageBuffer(size); }, std::invalid_argument);
//     }
//   });
// }

TEST(MultiplexerTest, MessageBufferRemaining) {
  rc::check("MessageBuffer::remaining", [](const uint8_t size, const uint8_t position) {
    const unique_ptr<uint8_t[]> data(new uint8_t[size]);
    const MessageBuffer buf(span(data.get(), size));
    const size_t actual = buf.remaining(position);
    if (position >= size) {
      ASSERT_EQ(actual, 0);
    } else {
      ASSERT_EQ(actual, (size - position));
    }
  });
}

TEST(MultiPlexerTest, MessageBufferWrite) {
  rc::check("MessageBuffer::write", [](const uint8_t buf_size, const uint8_t position, const uint8_t src_size) {
    std::cout << "buf_size: " << static_cast<int>(buf_size) << ", position: " << static_cast<int>(position)
              << ", src_size: " << static_cast<int>(src_size) << '\n';

    const unique_ptr<uint8_t[]> data(new uint8_t[buf_size]);
    MessageBuffer buf(span(data.get(), buf_size));
    const size_t remaining = buf.remaining(position);
    const unique_ptr<uint8_t[]> src = create_test_array(src_size);
    const size_t written = buf.write(position, span(src.get(), src_size));
    if (remaining >= src_size + sizeof(uint16_t)) {
      ASSERT_EQ(src_size + sizeof(uint16_t), written);
      const span<uint8_t> read = buf.read(position);
      ASSERT_EQ(read.size(), src_size);
      // compare read bytes with the src bytes
      for (uint8_t i = 0; i < src_size; i++) {
        std::cout << "\ti:" << static_cast<int>(i) << ", value: " << static_cast<int>(src.get()[i]) << '\n';
        ASSERT_EQ(read[i], src.get()[i]);
      }
    } else {
      ASSERT_EQ(0, written);
    }
  });
}

TEST(MultiPlexerTest, MessageBufferWriteEmpty) {
  const size_t size = TEST_MAX_MSG_SIZE + 2;
  const unique_ptr<uint8_t[]> data(new uint8_t[size]);
  MessageBuffer buf(span(data.get(), size));
  const unique_ptr<uint8_t[]> msg(new uint8_t[0]);
  const size_t written = buf.write(0, span(msg.get(), 0));
  ASSERT_EQ(written, sizeof(uint16_t));
  const span<uint8_t> read = buf.read(0);
  ASSERT_EQ(read.size(), 0);
}

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
TEST(MultiplexerTest, Constructor) {
  const MultiplexerPublisher<32, 4> m0(5);
  const MultiplexerPublisher<32, 4> m1(32);
  const MultiplexerPublisher<32, 4> m2(64);

  try {
    const MultiplexerPublisher<32, 4> m3(0);
    FAIL() << "Expected exception here";
  } catch (std::invalid_argument& e) {
    ASSERT_STREQ(e.what(), "subscriber_number must be within the inclusive interval: [1, 64]");
  }

  try {
    const MultiplexerPublisher<32, 4> m3(65);
    FAIL() << "Expected exception here";
  } catch (std::invalid_argument& e) {
    ASSERT_STREQ(e.what(), "subscriber_number must be within the inclusive interval: [1, 64]");
  }
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
