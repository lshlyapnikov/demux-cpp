#define UNIT_TEST

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner)

#include "../src/demultiplexer.h"
#include <gtest/gtest.h>
#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <array>
#include <atomic>
#include <boost/log/core.hpp>         // NOLINT(misc-include-cleaner)
#include <boost/log/expressions.hpp>  // NOLINT(misc-include-cleaner)
#include <boost/log/trivial.hpp>
#include <boost/serialization/strong_typedef.hpp>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <span>
#include <vector>
#include "../src/domain.h"
#include "../src/message_buffer.h"
#include "./domain_test.h"

namespace demux {

const std::chrono::seconds DEFAULT_WAIT(5);  // NOLINT(cert-err58-cpp)

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
BOOST_STRONG_TYPEDEF(std::vector<uint8_t>, TestMessage);

}  // namespace demux

using demux::DEFAULT_WAIT;
using demux::DemultiplexerPublisher;
using demux::DemultiplexerSubscriber;
using demux::SendResult;
using demux::SubscriberId;
using demux::TestMessage;
using std::array;
using std::atomic;
using std::span;
using std::uint16_t;
using std::uint8_t;
using std::vector;

constexpr size_t L = 128;
constexpr uint16_t M = 64;

namespace rc {

template <>
struct Arbitrary<TestMessage> {
  static auto arbitrary() -> Gen<TestMessage> {
    const Gen<vector<uint8_t>> nonempty_vec_gen = gen::resize(M, gen::nonEmpty<vector<uint8_t>>());
    // The above generator might still generate a vector with `size() > M`, `gen::resize` does not always work.
    // Run TestMessageGenerator.CheckDistribution1 500 times and check `RC_CLASSIFY(message_size > M, "size > M")`
    const Gen<vector<uint8_t>> valid_size_vec_gen = gen::suchThat(nonempty_vec_gen, is_valid_length);

    return gen::map(valid_size_vec_gen, [](const vector<uint8_t>& xs) { return TestMessage(xs); });
  }

  static auto is_valid_length(const vector<uint8_t>& xs) -> bool { return !xs.empty() && xs.size() <= M; }
};

}  // namespace rc

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

template <size_t L, uint16_t M, bool B>
auto send_all(const vector<TestMessage>& messages, DemultiplexerPublisher<L, M, B>& publisher) -> size_t {
  size_t result = 0;
  for (size_t i = 0; i < messages.size();) {
    TestMessage m = messages[i];
    switch (publisher.send(m.t)) {
      case SendResult::Success:
        i += 1;
        result += 1;
        break;
      case SendResult::Repeat:
        break;
      case SendResult::Error:
        return result;
    }
  }

  return result;
}

template <size_t L, uint16_t M>
auto read_n(const size_t message_num, DemultiplexerSubscriber<L, M>& subscriber) -> vector<TestMessage> {
  vector<TestMessage> result;
  while (result.size() < message_num) {
    const span<uint8_t>& m = subscriber.next();
    if (!m.empty()) {
      result.emplace_back(TestMessage(vector<uint8_t>{m.begin(), m.end()}));
    }
  }

  // read one more to unblock the subscriber, which might be waiting for the wraparound unblock
  const span<uint8_t>& m = subscriber.next();
  assert(m.empty());

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

template <bool Blocking>
auto publisher_constructor_does_not_throw(const uint8_t all_subs_mask) -> void {
  array<uint8_t, 32> buffer{};  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};

  const DemultiplexerPublisher<32, 4, Blocking> m(all_subs_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);
  ASSERT_EQ(0, m.message_count());
  ASSERT_EQ(0, m.position());
  ASSERT_EQ(all_subs_mask, m.all_subs_mask());
}

TEST(BlockingDemultiplexerPublisherTest, ConstructorDoesNotThrow) {
  rc::check(publisher_constructor_does_not_throw<true>);
}

TEST(NonBlockingDemultiplexerPublisherTest, ConstructorDoesNotThrow) {
  rc::check(publisher_constructor_does_not_throw<false>);
}

template <bool Blocking>
auto publisher_send_empty_message() {
  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint8_t all_subs_mask = 0b1;
  const SubscriberId subId = SubscriberId::create(1);

  DemultiplexerPublisher<L, M, Blocking> publisher(all_subs_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);
  DemultiplexerSubscriber<L, M> subscriber(subId, span{buffer}, &msg_counter_sync, &wraparound_sync);

  const SendResult result = publisher.send({});

  ASSERT_EQ(SendResult::Error, result);
  ASSERT_EQ(0, publisher.message_count());

  const span<uint8_t> read = subscriber.next();
  ASSERT_EQ(0, read.size());
  ASSERT_EQ(0, subscriber.message_count());
}

TEST(BlockingDemultiplexerPublisherTest, SendEmptyMessage) {
  publisher_send_empty_message<true>();
}

TEST(NonBlockingDemultiplexerPublisherTest, SendEmptyMessage) {
  publisher_send_empty_message<false>();
}

template <bool Blocking>
auto publisher_send_invalid_large_message() -> void {
  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint8_t all_subs_mask = 0b1;
  const SubscriberId subId = SubscriberId::create(1);

  DemultiplexerPublisher<L, M, Blocking> publisher(all_subs_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);
  DemultiplexerSubscriber<L, M> subscriber(subId, span{buffer}, &msg_counter_sync, &wraparound_sync);

  array<uint8_t, L> m{1};  // this should not fit into the buffer given M + 2 requirement
  const SendResult result = publisher.send(m);
  ASSERT_EQ(SendResult::Error, result);
  ASSERT_EQ(0, publisher.message_count());

  const span<uint8_t> read = subscriber.next();
  ASSERT_EQ(0, read.size());
  ASSERT_EQ(0, subscriber.message_count());
}

TEST(BlockingDemultiplexerPublisherTest, SendInvalidLargeMessage) {
  publisher_send_invalid_large_message<true>();
}

TEST(NonBlockingDemultiplexerPublisherTest, SendInvalidLargeMessage) {
  publisher_send_invalid_large_message<false>();
}

TEST(NonBlockingDeDemultiplexerPublisherTest, SendWhenBufferIfFullAndGetSendRepeatResult) {
  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint8_t all_subs_mask = 0b1;
  const SubscriberId subId = SubscriberId::create(1);

  DemultiplexerPublisher<L, M, false> publisher(all_subs_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);
  DemultiplexerSubscriber<L, M> subscriber(subId, span{buffer}, &msg_counter_sync, &wraparound_sync);

  ASSERT_EQ(L, M * 2);

  array<uint8_t, M> m1{1};
  const SendResult result1 = publisher.send(m1);
  ASSERT_EQ(SendResult::Success, result1);
  ASSERT_EQ(1, publisher.message_count());

  array<uint8_t, M> m2{2};
  const SendResult result2 = publisher.send(m2);
  ASSERT_EQ(SendResult::Repeat, result2);
  ASSERT_EQ(2, publisher.message_count());  // empty message written during the wraparound counts

  const span<uint8_t> read1 = subscriber.next();
  ASSERT_EQ(1, subscriber.message_count());
  assert_eq(m1, read1);

  const span<uint8_t> read2 = subscriber.next();
  ASSERT_EQ(0, read2.size());
}

template <bool Blocking>
auto publisher_send_and_receive_1(TestMessage message) {
  if (message.t.size() > M) {
    return;
  }
  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint8_t all_subs_mask = 0b1;
  const SubscriberId subId = SubscriberId::create(1);

  DemultiplexerPublisher<L, M, Blocking> publisher(all_subs_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);
  DemultiplexerSubscriber<L, M> subscriber(subId, span{buffer}, &msg_counter_sync, &wraparound_sync);

  const SendResult result = publisher.send(message.t);

  ASSERT_EQ(SendResult::Success, result);
  ASSERT_EQ(1, publisher.message_count());

  const span<uint8_t> read = subscriber.next();

  ASSERT_EQ(1, subscriber.message_count());
  assert_eq(read, message.t);
}

TEST(BlockingDemultiplexerPublisherTest, SendReceive1) {
  rc::check(publisher_send_and_receive_1<true>);
}

TEST(NonBlockingDemultiplexerPublisherTest, SendReceive1) {
  rc::check(publisher_send_and_receive_1<false>);
}

template <bool Blocking>
auto publisher_one_sub_receive_x(const vector<TestMessage>& valid_messages) {
  if (valid_messages.empty()) {
    return;
  }

  const size_t message_num = valid_messages.size();

  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint8_t all_subs_mask = 0b1;
  const SubscriberId subId = SubscriberId::create(1);

  DemultiplexerPublisher<L, M, Blocking> publisher(all_subs_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);
  DemultiplexerSubscriber<L, M> subscriber(subId, span{buffer}, &msg_counter_sync, &wraparound_sync);

  std::future<size_t> sent_count_future =
      std::async(std::launch::async, [&valid_messages, &publisher] { return send_all(valid_messages, publisher); });

  std::future<vector<TestMessage>> received_messages_future =
      std::async(std::launch::async, [message_num, &subscriber] { return read_n(message_num, subscriber); });

  sent_count_future.wait_for(DEFAULT_WAIT);
  ASSERT_TRUE(sent_count_future.valid());

  const size_t sent_count = sent_count_future.get();
  ASSERT_EQ(message_num, sent_count);

  received_messages_future.wait_for(DEFAULT_WAIT);
  ASSERT_TRUE(received_messages_future.valid());

  const auto received_messages = received_messages_future.get();
  assert_eq(valid_messages, received_messages);
}

TEST(BlockingDemultiplexerPublisherTest, OneSubReceiveX) {
  rc::check(publisher_one_sub_receive_x<true>);
}

TEST(NonBlockingDemultiplexerPublisherTest, OneSubReceiveX) {
  rc::check(publisher_one_sub_receive_x<false>);
}

template <bool Blocking>
auto publisher_multiple_subs_receive_x(const vector<TestMessage>& valid_messages) {
  if (valid_messages.empty()) {
    return;
  }

  constexpr uint8_t SUB_NUM = 7;

  const size_t message_num = valid_messages.size();

  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint64_t all_subs_mask = SubscriberId::all_subscribers_mask(SUB_NUM);

  vector<DemultiplexerSubscriber<L, M>> subscribers{};
  subscribers.reserve(SUB_NUM);
  for (uint8_t i = 1; i <= SUB_NUM; ++i) {
    const SubscriberId id = SubscriberId::create(i);
    const DemultiplexerSubscriber<L, M> sub{id, span{buffer}, &msg_counter_sync, &wraparound_sync};
    subscribers.emplace_back(sub);
  }

  DemultiplexerPublisher<L, M, Blocking> publisher(all_subs_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);

  std::future<size_t> future_pub_result =
      std::async(std::launch::async, [&valid_messages, &publisher] { return send_all(valid_messages, publisher); });

  vector<std::future<vector<TestMessage>>> future_sub_results{};
  future_sub_results.reserve(SUB_NUM);
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
}

TEST(BlockingDemultiplexerPublisherTest, MultipleSubsReceiveX) {
  rc::check(publisher_multiple_subs_receive_x<true>);
}

TEST(NonBlockingDemultiplexerPublisherTest, MultipleSubsReceiveX) {
  rc::check(publisher_multiple_subs_receive_x<false>);
}

TEST(TestMessageGenerator, CheckLengthDistribution) {
  GTEST_SKIP();
  rc::check([](const TestMessage& message) {
    const size_t message_size = message.t.size();
    // RC_CLASSIFY(message_size == 0, "size == 0");
    // RC_CLASSIFY(message_size == 1, "size == 1");
    // RC_CLASSIFY(0 < message_size && message_size <= 32, "0 < size <= 32");
    // RC_CLASSIFY(message_size > 32, "size > 32");
    // RC_CLASSIFY(message_size == M, "size == M");
    // RC_CLASSIFY(message_size > M, "size > M");
    RC_TAG(message_size);
  });
}

TEST(TestMessageGenerator, CheckByteDistribution) {
  GTEST_SKIP();
  rc::check([](const TestMessage& message) {
    for (const uint8_t x : message.t) {
      RC_TAG(x);
    }
  });
}

template <bool Blocking>
auto publisher_add_remove_subscriber(const vector<SubscriberId>& subs) {
  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  DemultiplexerPublisher<L, M, Blocking> publisher(0, span{buffer}, &msg_counter_sync, &wraparound_sync);

  ASSERT_EQ(0, publisher.all_subs_mask());

  for (auto sub : subs) {
    ASSERT_FALSE(publisher.is_subscriber(sub));
  }

  for (auto sub : subs) {
    publisher.add_subscriber(sub);
    ASSERT_TRUE(publisher.is_subscriber(sub));
    ASSERT_NE(0, publisher.all_subs_mask());
  }

  for (auto sub : subs) {
    publisher.remove_subscriber(sub);
    ASSERT_FALSE(publisher.is_subscriber(sub));
  }

  for (auto sub : subs) {
    ASSERT_FALSE(publisher.is_subscriber(sub));
  }

  ASSERT_EQ(0, publisher.all_subs_mask());
}

TEST(BlockingDemultiplexerPublisherTest, AddRemoveSubscriber) {
  rc::check(publisher_add_remove_subscriber<true>);
}

TEST(NonBlockingDemultiplexerPublisherTest, AddRemoveSubscriber) {
  rc::check(publisher_add_remove_subscriber<false>);
}

auto main(int argc, char** argv) -> int {
  namespace logging = boost::log;
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::warning);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)