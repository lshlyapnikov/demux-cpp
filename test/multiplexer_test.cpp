#define UNIT_TEST

#include "../src/multiplexer.h"
#include <gtest/gtest.h>
#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include "../src/domain.h"

using ShmSequencer::MessageBuffer;
using ShmSequencer::MultiplexerPublisher;
using ShmSequencer::MultiplexerSubscriber;
using ShmSequencer::SubscriberId;
using std::array;
using std::atomic;
using std::span;
using std::uint16_t;
using std::uint8_t;
using std::unique_ptr;
using std::vector;

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

TEST(MessageBufferTest, MessageBufferRemaining) {
  rc::check("MessageBuffer::remaining", [](const uint8_t position) {
    std::cout << "position: " << static_cast<int>(position) << '\n';

    constexpr size_t BUF_SIZE = 32;

    std::array<uint8_t, BUF_SIZE> data{0};
    const MessageBuffer<BUF_SIZE> buf(span<uint8_t, BUF_SIZE>{data});

    const size_t actual = buf.remaining(position);
    if (position >= BUF_SIZE) {
      ASSERT_EQ(actual, 0);
    } else {
      ASSERT_EQ(actual, (BUF_SIZE - position));
    }
  });
}

TEST(MessageBufferTest, WriteToSharedMemory) {
  rc::check("MessageBuffer::write", [](std::vector<uint8_t> message) {
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
        ASSERT_EQ(data[i + sizeof(uint16_t)], message[i]);
        ASSERT_EQ(read[i], message[i]);
      }
    } else {
      ASSERT_EQ(written, 0);
    }
  });
}

TEST(MessageBufferTest, MessageBufferWriteRead) {
  rc::check("MessageBuffer::write", [](const uint8_t position, const uint8_t src_size) {
    std::cout << "position: " << static_cast<int>(position) << ", src_size: " << static_cast<int>(src_size) << '\n';

    constexpr size_t BUF_SIZE = 32;

    std::array<uint8_t, BUF_SIZE> data{};
    MessageBuffer<BUF_SIZE> buf(data);

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

TEST(MultiplexerPublisherTest, ConstructorDoesNotThrow) {
  rc::check("MultiplexerPublisher::Constructor", [](const uint8_t all_subs_mask) {
    array<uint8_t, 32> buffer;
    atomic<uint64_t> msg_counter_sync{0};
    atomic<uint64_t> wraparound_sync{0};

    const MultiplexerPublisher<32, 4> m(all_subs_mask, buffer, &msg_counter_sync, &wraparound_sync);
    ASSERT_EQ(0, m.message_count());
    ASSERT_EQ(0, m.position());
    ASSERT_EQ(all_subs_mask, m.all_subs_mask());
  });
}

// NOLINTBEGIN(misc - include - cleaner, cppcoreguidelines - avoid - magic - numbers, readability - magic - numbers)
TEST(MultiplexerPublisherTest, Roundtrip1) {
  rc::check("MultiplexerPublisher::Roundtrip1", [](vector<uint8_t> message) {
    if (message.size() == 0) {
      return;
    }
    array<uint8_t, 128> buffer;
    atomic<uint64_t> msg_counter_sync{0};
    atomic<uint64_t> wraparound_sync{0};
    const uint8_t all_subs_mask = 0b1;
    const SubscriberId subId = SubscriberId::create(1);

    MultiplexerPublisher<128, 64> publisher(all_subs_mask, buffer, &msg_counter_sync, &wraparound_sync);
    MultiplexerSubscriber<128, 64> subscriber(subId, buffer, &msg_counter_sync, &wraparound_sync);

    const bool ok = publisher.send(span<uint8_t>{message.data(), message.size()});
    ASSERT_TRUE(ok);
    ASSERT_EQ(1, publisher.message_count());

    const span<uint8_t> read = subscriber.next();
    ASSERT_EQ(read.size(), message.size());
    ASSERT_EQ(1, subscriber.message_count());

    for (size_t i = 0; i < message.size(); ++i) {
      ASSERT_EQ(read[i], message[i]);
    }
  });
}
// NOLINTEND(misc - include - cleaner, cppcoreguidelines - avoid - magic - numbers, readability - magic - numbers)

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
