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
#include "../src/message_buffer.h"

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

constexpr size_t L = 128;
constexpr uint16_t M = 64;

namespace rc {

template <>
struct Arbitrary<TestMessage> {
  static Gen<TestMessage> arbitrary() {
    Gen<vector<uint8_t>> nonempty_vec_gen = gen::resize(M, gen::nonEmpty<vector<uint8_t>>());
    // The above generator might still generate a vector with `size() > M`, `gen::resize` does not always work.
    // Run TestMessageGenerator.CheckDistribution1 500 times and check `RC_CLASSIFY(message_size > M, "size > M")`
    Gen<vector<uint8_t>> valid_size_vec_gen =
        gen::suchThat(nonempty_vec_gen, [](vector<uint8_t> xs) { return xs.size() > 0 && xs.size() <= M; });

    return gen::map(valid_size_vec_gen, [](vector<uint8_t> xs) { return TestMessage(xs); });
  }
};

}  // namespace rc

auto assert_eq(const span<uint8_t>& left, const span<uint8_t>& right) {
  ASSERT_EQ(left.size(), right.size());
  for (size_t i = 0; i < right.size(); ++i) {
    ASSERT_EQ(left[i], right[i]) << "index: " << i;
  }
}

auto filter_valid_messages(const vector<TestMessage> messages) -> vector<TestMessage> {
  // vector<TestMessage> result;
  // for (auto m : messages) {
  //   if (0 < m.t.size() && m.t.size() <= M) {
  //     result.emplace_back(m);
  //   }
  // }
  // return result;
  return messages;
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
    if (message.t.size() > M) {
      return;
    }
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
auto send_all(const vector<TestMessage>& messages, MultiplexerPublisher<L, M>& publisher) -> size_t {
  size_t result = 0;
  for (TestMessage m : messages) {
    const bool ok = publisher.send(m.t);
    if (ok) {
      result += 1;
    }
  }
  assert(result == messages.size());
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
  rc::check([](const vector<TestMessage>& messages) {
    const vector<TestMessage> valid_messages = filter_valid_messages(messages);

    if (valid_messages.empty()) {
      return;
    }

    const size_t message_num = valid_messages.size();

    array<uint8_t, L> buffer;
    atomic<uint64_t> msg_counter_sync{0};
    atomic<uint64_t> wraparound_sync{0};
    const uint8_t all_subs_mask = 0b1;
    const SubscriberId subId = SubscriberId::create(1);

    MultiplexerPublisher<L, M> publisher(all_subs_mask, buffer, &msg_counter_sync, &wraparound_sync);
    MultiplexerSubscriber<L, M> subscriber(subId, buffer, &msg_counter_sync, &wraparound_sync);

    std::future<size_t> sent_count_future =
        std::async(std::launch::async, [&valid_messages, &publisher] { return send_all(valid_messages, publisher); });

    std::future<vector<TestMessage>> received_messages_future =
        std::async(std::launch::async, [message_num, &subscriber] { return read_n(message_num, subscriber); });

    sent_count_future.wait_for(DEFAULT_WAIT);
    ASSERT_TRUE(sent_count_future.valid());

    received_messages_future.wait_for(DEFAULT_WAIT);
    ASSERT_TRUE(received_messages_future.valid());

    const size_t sent_count = sent_count_future.get();
    ASSERT_EQ(message_num, sent_count);

    const auto received_messages = received_messages_future.get();
    assert_eq(valid_messages, received_messages);
  });
}

TEST(MultiplexerPublisherTest, MultipleSubsReceiveX) {
  rc::check([](const vector<TestMessage>& messages) {
    const vector<TestMessage> valid_messages = filter_valid_messages(messages);

    if (valid_messages.empty()) {
      return;
    }

    constexpr uint8_t SUB_NUM = 7;

    const size_t message_num = valid_messages.size();

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

    std::future<size_t> future_pub_result =
        std::async(std::launch::async, [&valid_messages, &publisher] { return send_all(valid_messages, publisher); });

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
      assert_eq(valid_messages, future_sub_result.get());
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
    RC_CLASSIFY(message_size == M, "size == M");
    RC_CLASSIFY(message_size > M, "size > M");
  });
}

auto main(int argc, char** argv) -> int {
  namespace logging = boost::log;
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::warning);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}