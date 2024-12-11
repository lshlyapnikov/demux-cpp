// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner)

#define UNIT_TEST
#undef NDEBUG  // for assert to work in release build

#include "../core/demultiplexer.h"
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
#include "../core/message_buffer.h"
#include "../core/reader_id.h"
#include "./reader_id_gen.h"

namespace lshl::demux::core {

const std::chrono::seconds DEFAULT_WAIT(5);  // NOLINT(cert-err58-cpp)

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions,hicpp-special-member-functions)
BOOST_STRONG_TYPEDEF(std::vector<uint8_t>, TestMessage);

}  // namespace lshl::demux::core

using lshl::demux::core::DEFAULT_WAIT;
using lshl::demux::core::DemuxReader;
using lshl::demux::core::DemuxWriter;
using lshl::demux::core::ReaderId;
using lshl::demux::core::TestMessage;
using lshl::demux::core::WriteResult;
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
auto write_all(const vector<TestMessage>& messages, DemuxWriter<L, M, B>& writer) -> size_t {
  size_t result = 0;
  for (size_t i = 0; i < messages.size();) {
    TestMessage m = messages[i];
    switch (writer.write(m.t)) {
      case WriteResult::Success:
        i += 1;
        result += 1;
        break;
      case WriteResult::Repeat:
        break;
      case WriteResult::Error:
        return result;
    }
  }

  return result;
}

template <size_t L, uint16_t M>
auto read_n(const size_t message_num, DemuxReader<L, M>& reader) -> vector<TestMessage> {
  vector<TestMessage> result;
  while (result.size() < message_num) {
    const span<uint8_t>& m = reader.next();
    if (!m.empty()) {
      result.emplace_back(TestMessage(vector<uint8_t>{m.begin(), m.end()}));
    }
  }

  // read one more to unblock the reader, which might be waiting for the wraparound unblock
  const span<uint8_t>& m = reader.next();
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
auto writer_constructor_does_not_throw(const uint8_t all_readers_mask) -> void {
  array<uint8_t, 32> buffer{};  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};

  const DemuxWriter<32, 4, Blocking> m(all_readers_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);
  ASSERT_EQ(0, m.message_count());
  ASSERT_EQ(0, m.position());
  ASSERT_EQ(all_readers_mask, m.all_readers_mask());
}

TEST(BlockingDemuxWriterTest, ConstructorDoesNotThrow) {
  rc::check(writer_constructor_does_not_throw<true>);
}

TEST(NonBlockingDemuxWriterTest, ConstructorDoesNotThrow) {
  rc::check(writer_constructor_does_not_throw<false>);
}

template <bool Blocking>
auto write_empty_message() {
  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint8_t all_readers_mask = 0b1;
  const ReaderId subId = ReaderId::create(1);

  DemuxWriter<L, M, Blocking> writer(all_readers_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);
  DemuxReader<L, M> reader(subId, span{buffer}, &msg_counter_sync, &wraparound_sync);

  // write an empty message
  const WriteResult result = writer.write({});

  ASSERT_EQ(WriteResult::Error, result);
  ASSERT_EQ(0, writer.message_count());

  const span<uint8_t> read = reader.next();
  ASSERT_EQ(0, read.size());
  ASSERT_EQ(0, reader.message_count());
}

TEST(BlockingDemuxWriterTest, WriteEmptyMessage) {
  write_empty_message<true>();
}

TEST(NonBlockingDemuxWriterTest, WriteEmptyMessage) {
  write_empty_message<false>();
}

template <bool Blocking>
auto write_invalid_large_message() -> void {
  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint8_t all_readers_mask = 0b1;
  const ReaderId subId = ReaderId::create(1);

  DemuxWriter<L, M, Blocking> writer(all_readers_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);
  DemuxReader<L, M> reader(subId, span{buffer}, &msg_counter_sync, &wraparound_sync);

  array<uint8_t, L> m{1};  // this should not fit into the buffer given M + 2 requirement
  const WriteResult result = writer.write(m);
  ASSERT_EQ(WriteResult::Error, result);
  ASSERT_EQ(0, writer.message_count());

  const span<uint8_t> read = reader.next();
  ASSERT_EQ(0, read.size());
  ASSERT_EQ(0, reader.message_count());
}

TEST(BlockingDemuxWriterTest, WriteInvalidLargeMessage) {
  write_invalid_large_message<true>();
}

TEST(NonBlockingDemuxWriterTest, WriteInvalidLargeMessage) {
  write_invalid_large_message<false>();
}

TEST(NonBlockingDemuxWriterTest, WriteWhenBufferIfFullAndGetWriteRepeatResult) {
  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint8_t all_readers_mask = 0b1;
  const ReaderId subId = ReaderId::create(1);

  DemuxWriter<L, M, false> writer(all_readers_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);
  DemuxReader<L, M> reader(subId, span{buffer}, &msg_counter_sync, &wraparound_sync);

  ASSERT_EQ(L, M * 2);

  array<uint8_t, M> m1{1};
  const WriteResult result1 = writer.write(m1);
  ASSERT_EQ(WriteResult::Success, result1);
  ASSERT_EQ(1, writer.message_count());

  array<uint8_t, M> m2{2};
  const WriteResult result2 = writer.write(m2);
  ASSERT_EQ(WriteResult::Repeat, result2);
  ASSERT_EQ(2, writer.message_count());  // empty message written during the wraparound counts

  const span<uint8_t> read1 = reader.next();
  ASSERT_EQ(1, reader.message_count());
  assert_eq(m1, read1);

  const span<uint8_t> read2 = reader.next();
  ASSERT_EQ(0, read2.size());
}

template <bool Blocking>
auto write_and_read_1(TestMessage message) {
  if (message.t.size() > M) {
    return;
  }
  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint8_t all_readers_mask = 0b1;
  const ReaderId subId = ReaderId::create(1);

  DemuxWriter<L, M, Blocking> writer(all_readers_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);
  DemuxReader<L, M> reader(subId, span{buffer}, &msg_counter_sync, &wraparound_sync);

  const WriteResult result = writer.write(message.t);

  ASSERT_EQ(WriteResult::Success, result);
  ASSERT_EQ(1, writer.message_count());

  const span<uint8_t> read = reader.next();

  ASSERT_EQ(1, reader.message_count());
  assert_eq(read, message.t);
}

TEST(BlockingDemuxWriterTest, WriteRead1) {
  rc::check(write_and_read_1<true>);
}

TEST(NonBlockingDemuxWriterTest, WriteRead1) {
  rc::check(write_and_read_1<false>);
}

template <bool Blocking>
auto one_reader_read_x(const vector<TestMessage>& valid_messages) {
  if (valid_messages.empty()) {
    return;
  }

  const size_t message_num = valid_messages.size();

  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint8_t all_readers_mask = 0b1;
  const ReaderId subId = ReaderId::create(1);

  DemuxWriter<L, M, Blocking> writer(all_readers_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);
  DemuxReader<L, M> reader(subId, span{buffer}, &msg_counter_sync, &wraparound_sync);

  std::future<size_t> sent_count_future =
      std::async(std::launch::async, [&valid_messages, &writer] { return write_all(valid_messages, writer); });

  std::future<vector<TestMessage>> received_messages_future =
      std::async(std::launch::async, [message_num, &reader] { return read_n(message_num, reader); });

  sent_count_future.wait_for(DEFAULT_WAIT);
  ASSERT_TRUE(sent_count_future.valid());

  const size_t sent_count = sent_count_future.get();
  ASSERT_EQ(message_num, sent_count);

  received_messages_future.wait_for(DEFAULT_WAIT);
  ASSERT_TRUE(received_messages_future.valid());

  const auto received_messages = received_messages_future.get();
  assert_eq(valid_messages, received_messages);
}

TEST(BlockingDemuxWriterTest, OneReaderReadX) {
  rc::check(one_reader_read_x<true>);
}

TEST(NonBlockingDemuxWriterTest, OneReaderReadX) {
  rc::check(one_reader_read_x<false>);
}

template <bool Blocking>
auto multiple_readers_read_x(const vector<TestMessage>& valid_messages) {
  if (valid_messages.empty()) {
    return;
  }

  constexpr uint8_t SUB_NUM = 7;

  const size_t message_num = valid_messages.size();

  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint64_t all_readers_mask = ReaderId::all_readers_mask(SUB_NUM);

  vector<DemuxReader<L, M>> readers{};
  readers.reserve(SUB_NUM);
  for (uint8_t i = 1; i <= SUB_NUM; ++i) {
    const ReaderId id = ReaderId::create(i);
    const DemuxReader<L, M> sub{id, span{buffer}, &msg_counter_sync, &wraparound_sync};
    readers.emplace_back(sub);
  }

  DemuxWriter<L, M, Blocking> writer(all_readers_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);

  std::future<size_t> future_pub_result =
      std::async(std::launch::async, [&valid_messages, &writer] { return write_all(valid_messages, writer); });

  vector<std::future<vector<TestMessage>>> future_sub_results{};
  future_sub_results.reserve(SUB_NUM);
  for (auto& reader : readers) {
    future_sub_results.emplace_back(std::async(std::launch::async, [message_num, &reader] {
      return read_n(message_num, reader);
    }));
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

TEST(BlockingDemuxWriterTest, MultipleReadersReadX) {
  rc::check(multiple_readers_read_x<true>);
}

TEST(NonBlockingDemuxWriterTest, MultipleReadersReadX) {
  rc::check(multiple_readers_read_x<false>);
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
auto writer_add_remove_reader(const vector<ReaderId>& subs) {
  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  DemuxWriter<L, M, Blocking> writer(0, span{buffer}, &msg_counter_sync, &wraparound_sync);

  ASSERT_EQ(0, writer.all_readers_mask());

  for (auto sub : subs) {
    ASSERT_FALSE(writer.is_reader(sub));
  }

  for (auto sub : subs) {
    writer.add_reader(sub);
    ASSERT_TRUE(writer.is_reader(sub));
    ASSERT_NE(0, writer.all_readers_mask());
  }

  for (auto sub : subs) {
    writer.remove_reader(sub);
    ASSERT_FALSE(writer.is_reader(sub));
  }

  for (auto sub : subs) {
    ASSERT_FALSE(writer.is_reader(sub));
  }

  ASSERT_EQ(0, writer.all_readers_mask());
}

TEST(BlockingDemuxWriterTest, AddRemoveReader) {
  rc::check(writer_add_remove_reader<true>);
}

TEST(NonBlockingDemuxWriterTest, AddRemoveReader) {
  rc::check(writer_add_remove_reader<false>);
}

auto main(int argc, char** argv) -> int {
  namespace logging = boost::log;
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::warning);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)