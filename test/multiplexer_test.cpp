#define UNIT_TEST

#include "../src/multiplexer.h"
#include <gtest/gtest.h>
#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <array>
#include <atomic>
#include <boost/log/core.hpp>         // NOLINT(misc-include-cleaner)
#include <boost/log/expressions.hpp>  // NOLINT(misc-include-cleaner)
#include <boost/log/trivial.hpp>
#include <boost/serialization/strong_typedef.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <span>
#include <vector>
#include "../src/domain.h"

namespace ShmSequencer {

const std::chrono::seconds DEFAULT_WAIT(5);

BOOST_STRONG_TYPEDEF(std::vector<uint8_t>, TestMessage);

}  // namespace ShmSequencer

using ShmSequencer::DEFAULT_WAIT;
using ShmSequencer::MessageBuffer;
using ShmSequencer::MultiplexerPublisher;
using ShmSequencer::MultiplexerSubscriber;
using ShmSequencer::SubscriberId;
using ShmSequencer::TestMessage;
using std::array;
using std::atomic;
using std::span;
using std::uint16_t;
using std::uint8_t;
using std::unique_ptr;
using std::vector;

namespace rc {

template <>
struct Arbitrary<TestMessage> {
  static Gen<TestMessage> arbitrary() {
    Gen<vector<uint8_t>> nonempty_vec_gen = gen::nonEmpty<vector<uint8_t>>();
    return gen::map(nonempty_vec_gen, [](vector<uint8_t> xs) { return TestMessage(xs); });
  }
};

}  // namespace rc

auto create_test_array(const size_t size) -> unique_ptr<uint8_t[]> {
  unique_ptr<uint8_t[]> result(new uint8_t[size]);
  for (uint8_t i = 0; i < size; i++) {
    result.get()[i] = i;
  }
  return result;
}

auto assert_eq(const span<uint8_t>& left, const span<uint8_t>& right) {
  ASSERT_EQ(left.size(), right.size());
  for (size_t i = 0; i < right.size(); ++i) {
    ASSERT_EQ(left[i], right[i]) << "index: " << i;
  }
}

auto assert_eq(const TestMessage& left, const TestMessage& right) {
  ASSERT_EQ(left.t.size(), right.t.size());
  for (size_t i = 0; i < right.t.size(); ++i) {
    ASSERT_EQ(left.t[i], right.t[i]) << "index: " << i;
  }
}

auto assert_eq(const vector<TestMessage>& left, const vector<TestMessage>& right) {
  ASSERT_EQ(left.size(), right.size());
  for (size_t i = 0; i < right.size(); ++i) {
    const TestMessage& x = left[i];
    const TestMessage& y = right[i];
    assert_eq(x, y);
  }
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
  rc::check([](const uint8_t position) {
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
        ASSERT_EQ(data[i + sizeof(uint16_t)], message[i]);
        ASSERT_EQ(read[i], message[i]);
      }
    } else {
      ASSERT_EQ(written, 0);
    }
  });
}

TEST(MessageBufferTest, MessageBufferWriteRead) {
  rc::check([](const uint8_t position, const uint8_t src_size) {
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
  rc::check([](const uint8_t all_subs_mask) {
    array<uint8_t, 32> buffer;
    atomic<uint64_t> msg_counter_sync{0};
    atomic<uint64_t> wraparound_sync{0};

    const MultiplexerPublisher<32, 4> m(all_subs_mask, buffer, &msg_counter_sync, &wraparound_sync);
    ASSERT_EQ(0, m.message_count());
    ASSERT_EQ(0, m.position());
    ASSERT_EQ(all_subs_mask, m.all_subs_mask());
  });
}

TEST(MultiplexerPublisherTest, SendEmptyMessage) {
  constexpr size_t L = 128;
  constexpr uint16_t M = 64;

  array<uint8_t, L> buffer;
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint8_t all_subs_mask = 0b1;
  const SubscriberId subId = SubscriberId::create(1);

  MultiplexerPublisher<L, M> publisher(all_subs_mask, buffer, &msg_counter_sync, &wraparound_sync);
  MultiplexerSubscriber<L, M> subscriber(subId, buffer, &msg_counter_sync, &wraparound_sync);

  const bool ok = publisher.send({});

  ASSERT_FALSE(ok);
  ASSERT_EQ(0, publisher.message_count());

  const span<uint8_t> read = subscriber.next();
  ASSERT_EQ(0, read.size());
  ASSERT_EQ(0, subscriber.message_count());
}

// NOLINTBEGIN(misc - include - cleaner, cppcoreguidelines - avoid - magic - numbers, readability - magic - numbers)
TEST(MultiplexerPublisherTest, SendReceive1) {
  rc::check([](TestMessage message) {
    constexpr size_t L = 128;
    constexpr uint16_t M = 64;

    array<uint8_t, L> buffer;
    atomic<uint64_t> msg_counter_sync{0};
    atomic<uint64_t> wraparound_sync{0};
    const uint8_t all_subs_mask = 0b1;
    const SubscriberId subId = SubscriberId::create(1);

    MultiplexerPublisher<L, M> publisher(all_subs_mask, buffer, &msg_counter_sync, &wraparound_sync);
    MultiplexerSubscriber<L, M> subscriber(subId, buffer, &msg_counter_sync, &wraparound_sync);

    const bool ok = publisher.send(message.t);

    ASSERT_TRUE(ok);
    ASSERT_EQ(1, publisher.message_count());

    const span<uint8_t> read = subscriber.next();

    ASSERT_EQ(1, subscriber.message_count());
    assert_eq(read, message.t);
  });
}
// NOLINTEND(misc - include - cleaner, cppcoreguidelines - avoid - magic - numbers, readability - magic - numbers)

template <size_t L, uint16_t M>
auto send_all(const vector<TestMessage>& nonempty_messages, MultiplexerPublisher<L, M>& publisher) -> size_t {
  size_t result = 0;
  for (auto m : nonempty_messages) {
    const bool ok = publisher.send(m.t);
    if (ok) {
      result += 1;
    }
  }
  return result;
}

template <size_t L, uint16_t M>
auto read_n(const size_t message_num, MultiplexerSubscriber<L, M>& subscriber) -> vector<TestMessage> {
  vector<TestMessage> result;
  while (result.size() < message_num) {
    const span<uint8_t>& m = subscriber.next();
    if (!m.empty()) {
      result.emplace_back(TestMessage(vector<uint8_t>{m.begin(), m.end()}));
    }
  }

  // read one more to unblock the subscriber, which might be waiting for wraparound unblock
  const span<uint8_t>& m = subscriber.next();
  assert(0 == m.size());

  return result;
}

TEST(MultiplexerPublisherTest, SendReceiveX) {
  rc::check([](const vector<TestMessage>& nonempty_messages) {
    if (nonempty_messages.empty()) {
      return;
    }

    constexpr size_t L = 128;
    constexpr uint16_t M = 64;

    const size_t message_num = nonempty_messages.size();

    array<uint8_t, L> buffer;
    atomic<uint64_t> msg_counter_sync{0};
    atomic<uint64_t> wraparound_sync{0};
    const uint8_t all_subs_mask = 0b1;
    const SubscriberId subId = SubscriberId::create(1);

    MultiplexerPublisher<L, M> publisher(all_subs_mask, buffer, &msg_counter_sync, &wraparound_sync);
    MultiplexerSubscriber<L, M> subscriber(subId, buffer, &msg_counter_sync, &wraparound_sync);

    std::future<size_t> sent_count_future = std::async(
        std::launch::async, [&nonempty_messages, &publisher] { return send_all(nonempty_messages, publisher); });

    std::future<vector<TestMessage>> received_messages_future =
        std::async(std::launch::async, [message_num, &subscriber] { return read_n(message_num, subscriber); });

    sent_count_future.wait_for(DEFAULT_WAIT);
    ASSERT_TRUE(sent_count_future.valid());

    received_messages_future.wait_for(DEFAULT_WAIT);
    ASSERT_TRUE(received_messages_future.valid());

    const size_t sent_count = sent_count_future.get();
    ASSERT_EQ(message_num, sent_count);

    const auto received_messages = received_messages_future.get();
    assert_eq(nonempty_messages, received_messages);
  });
}

TEST(MultiplexerPublisherTest, MultipleSubsReceiveX) {
  // GTEST_SKIP();
  rc::check([](const vector<TestMessage>& nonempty_messages) {
    if (nonempty_messages.empty()) {
      return;
    }

    constexpr size_t L = 128;
    constexpr uint16_t M = 64;

    constexpr uint8_t SUB_NUM = 7;

    const size_t message_num = nonempty_messages.size();

    array<uint8_t, L> buffer;
    atomic<uint64_t> msg_counter_sync{0};
    atomic<uint64_t> wraparound_sync{0};
    const uint64_t all_subs_mask = SubscriberId::all_subscribers_mask(SUB_NUM);

    vector<MultiplexerSubscriber<L, M>> subscribers;
    for (uint8_t i = 1; i <= SUB_NUM; ++i) {
      subscribers.emplace_back(
          MultiplexerSubscriber<L, M>(SubscriberId::create(i), buffer, &msg_counter_sync, &wraparound_sync));
    }

    MultiplexerPublisher<L, M> publisher(all_subs_mask, buffer, &msg_counter_sync, &wraparound_sync);

    std::future<size_t> future_pub_result = std::async(
        std::launch::async, [&nonempty_messages, &publisher] { return send_all(nonempty_messages, publisher); });

    vector<std::future<vector<TestMessage>>> future_sub_results;
    for (auto& subscriber : subscribers) {
      future_sub_results.emplace_back(
          std::async(std::launch::async, [message_num, &subscriber] { return read_n(message_num, subscriber); }));
    }

    future_pub_result.wait_for(DEFAULT_WAIT);
    ASSERT_TRUE(future_pub_result.valid());
    ASSERT_EQ(message_num, future_pub_result.get());

    for (auto& future_sub_result : future_sub_results) {
      future_sub_result.wait_for(DEFAULT_WAIT);
      ASSERT_TRUE(future_sub_result.valid());
      assert_eq(nonempty_messages, future_sub_result.get());
    }
  });
}

TEST(TestMessageGenerator, CheckDistribution1) {
  rc::check([](TestMessage message) {
    const size_t message_size = message.t.size();
    RC_CLASSIFY(message_size == 0, "size == 0");
    RC_CLASSIFY(message_size == 1, "size == 1");
    RC_CLASSIFY(0 < message_size && message_size <= 32, "0 < size <= 32");
    RC_CLASSIFY(message_size > 32, "size > 32");
  });
}

auto main(int argc, char** argv) -> int {
  namespace logging = boost::log;
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::debug);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}