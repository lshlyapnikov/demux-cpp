// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner)

#include <rapidcheck/Check.h>
#include <rapidcheck/gen/Arbitrary.hpp>
#define UNIT_TEST
#undef NDEBUG  // for assert to work in release build

#include <gtest/gtest.h>
#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <array>
#include <atomic>
#include <boost/log/core.hpp>         // NOLINT(misc-include-cleaner)
#include <boost/log/expressions.hpp>  // NOLINT(misc-include-cleaner)
#include <boost/serialization/strong_typedef.hpp>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <limits>
#include <span>
#include <vector>
#include "../core/demux_reader.h"
#include "../core/demux_writer.h"
#include "../core/message_buffer.h"
#include "../core/reader_id.h"
#include "./demux_setup.h"
#include "./reader_id_gen.h"

namespace lshl::demux::core {

// explicitly reference the generator to make clangd happy
using _ = rc::Arbitrary<lshl::demux::core::ReaderId>;

constexpr std::chrono::seconds DEFAULT_WAIT(5);

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

using lshl::demux::core::test::DemuxSetup;

constexpr size_t L = 128;
constexpr uint16_t M = 64;

namespace rc {

template <>
struct Arbitrary<TestMessage> {
  static auto arbitrary() -> Gen<TestMessage> {
    // looks like gen::resize also affects the generated uint8_t values, use the entire byte range [0, 255]
    // enable TEST(TestMessageGenerator, CheckByteDistribution)
    const Gen<vector<uint8_t>> nonempty_vec_with_improved_distribution_gen =
        gen::resize(std::numeric_limits<uint8_t>::max(), gen::nonEmpty<vector<uint8_t>>());
    const Gen<vector<uint8_t>> nonempty_vec_gen = gen::resize(M, nonempty_vec_with_improved_distribution_gen);
    // The above generator might still generate a vector with `size() > M`, `gen::resize` does not always work.
    // Run TestMessageGenerator.CheckDistribution1 500 times and check `RC_CLASSIFY(message_size > M, "size > M")`
    const Gen<vector<uint8_t>> valid_size_vec_gen = gen::suchThat(nonempty_vec_gen, is_valid_length);

    return gen::map(valid_size_vec_gen, [](const vector<uint8_t>& xs) { return TestMessage(xs); });
  }

  static auto is_valid_length(const vector<uint8_t>& xs) -> bool { return !xs.empty() && xs.size() <= M; }
};

}  // namespace rc

namespace {
auto assert_eq(const span<uint8_t>& left, const span<uint8_t>& right) {
  ASSERT_EQ(left.size(), right.size());
  for (size_t i = 0; i < right.size(); ++i) {
    ASSERT_EQ(left[i], right[i]) << "index: " << i;
  }
}

[[nodiscard]] auto expect_eq(const span<uint8_t>& left, const span<uint8_t>& right) -> bool {
  EXPECT_EQ(left.size(), right.size());
  for (size_t i = 0; i < right.size(); ++i) {
    EXPECT_EQ(left[i], right[i]) << "index: " << i;
    if (::testing::Test::HasFailure()) {
      return false;
    }
  }
  return !::testing::Test::HasFailure();
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
auto write_all(const vector<TestMessage>& messages, DemuxWriter<L, M, B>* writer) -> size_t {
  size_t result = 0;
  for (size_t i = 0; i < messages.size();) {
    TestMessage m = messages[i];
    switch (writer->write(m.t)) {
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
auto read_n(const size_t message_num, DemuxReader<L, M>* reader) -> vector<TestMessage> {
  vector<TestMessage> result;
  while (result.size() < message_num) {
    const span<uint8_t>& m = reader->next();
    if (!m.empty()) {
      result.emplace_back(TestMessage(vector<uint8_t>{m.begin(), m.end()}));
    }
  }

  // read one more to unblock the reader, which might be waiting for the wraparound unblock
  const span<uint8_t>& m = reader->next();
  assert(m.empty());

  return result;
}

}  // namespace

TEST(MultiplexerTest, Atomic) {
  ASSERT_EQ(std::atomic<uint8_t>{}.is_lock_free(), true);
  ASSERT_EQ(std::atomic<uint16_t>{}.is_lock_free(), true);
  ASSERT_EQ(std::atomic<uint32_t>{}.is_lock_free(), true);
  ASSERT_EQ(std::atomic<size_t>{}.is_lock_free(), true);
  ASSERT_EQ(std::atomic<uint64_t>{}.is_lock_free(), true);
  ASSERT_EQ(sizeof(size_t), sizeof(uint64_t));
}

namespace {
template <bool Blocking>
auto writer_constructor_does_not_throw(uint8_t reader_num) -> void {
  DemuxSetup<L, M, Blocking> setup{reader_num};
  DemuxWriter<L, M, Blocking>* writer = setup.writer();

  ASSERT_EQ(0, writer->message_count());
  ASSERT_EQ(0, writer->downstream_sequence());
  ASSERT_EQ(0, writer->position());

  const vector<uint64_t> expected(reader_num, 0);
  vector<uint64_t> actual{};
  writer->upstream_sequences(&actual);
  ASSERT_EQ(expected, actual);
}
}  // namespace

TEST(BlockingDemuxWriterTest, ConstructorDoesNotThrow) {
  rc::check(writer_constructor_does_not_throw<true>);
}

TEST(NonBlockingDemuxWriterTest, ConstructorDoesNotThrow) {
  rc::check(writer_constructor_does_not_throw<false>);
}

namespace {
template <bool Blocking>
auto write_empty_message() {
  DemuxSetup<L, M, Blocking> setup{1};
  DemuxWriter<L, M, Blocking>* writer = setup.writer();
  DemuxReader<L, M>* reader = setup.reader(0);
  const ReaderId reader_id(0);

  ASSERT_TRUE(reader->is_id(reader_id));
  ASSERT_TRUE(reader->id() == reader_id);

  ASSERT_FALSE(reader->is_id(ReaderId(2)));
  ASSERT_FALSE(reader->id() == ReaderId(2));

  // write an empty message
  const WriteResult result = writer->write({});

  ASSERT_EQ(WriteResult::Error, result);
  ASSERT_EQ(0, writer->message_count());
  ASSERT_EQ(0, writer->downstream_sequence());

  const span<uint8_t> read = reader->next();
  ASSERT_EQ(0, read.size());
  ASSERT_EQ(0, reader->message_count());
}
}  // namespace

TEST(BlockingDemuxWriterTest, WriteEmptyMessage) {
  write_empty_message<true>();
}

TEST(NonBlockingDemuxWriterTest, WriteEmptyMessage) {
  write_empty_message<false>();
}

namespace {
template <bool Blocking>
auto write_invalid_large_message() -> void {
  DemuxSetup<L, M, Blocking> setup{1};
  DemuxWriter<L, M, Blocking>* writer = setup.writer();
  DemuxReader<L, M>* reader = setup.reader(0);

  array<uint8_t, L> m{1};  // this should not fit into the buffer given M + 2 requirement
  const WriteResult result = writer->write(m);
  ASSERT_EQ(WriteResult::Error, result);
  ASSERT_EQ(0, writer->message_count());

  const span<uint8_t> read = reader->next();
  ASSERT_EQ(0, read.size());
  ASSERT_EQ(0, reader->message_count());
}
}  // namespace

TEST(BlockingDemuxWriterTest, WriteInvalidLargeMessage) {
  write_invalid_large_message<true>();
}

TEST(NonBlockingDemuxWriterTest, WriteInvalidLargeMessage) {
  write_invalid_large_message<false>();
}

TEST(NonBlockingDemuxWriterTest, WriteWhenBufferIfFullAndGetWriteRepeatResult) {
  DemuxSetup<L, M, false> setup{1};
  DemuxWriter<L, M, false>* writer = setup.writer();
  DemuxReader<L, M>* reader = setup.reader(0);

  ASSERT_EQ(L, M * 2);

  array<uint8_t, M> m1{1};
  const WriteResult result1 = writer->write(m1);
  ASSERT_EQ(WriteResult::Success, result1);
  ASSERT_EQ(1, writer->message_count());
  ASSERT_EQ(1, writer->downstream_sequence());

  array<uint8_t, M> m2{2};
  const WriteResult result2 = writer->write(m2);
  ASSERT_EQ(WriteResult::Repeat, result2);
  ASSERT_EQ(2, writer->message_count());  // empty message written during the wraparound counts
  ASSERT_EQ(2, writer->downstream_sequence());

  const span<uint8_t> read1 = reader->next();
  ASSERT_EQ(1, reader->message_count());
  assert_eq(m1, read1);

  const span<uint8_t> read2 = reader->next();
  ASSERT_EQ(0, read2.size());
}

namespace {
template <bool Blocking>
auto write_and_read_1(TestMessage message) {
  if (message.t.size() > M) {
    return;
  }
  DemuxSetup<L, M, Blocking> setup{1};
  DemuxWriter<L, M, Blocking>* writer = setup.writer();
  DemuxReader<L, M>* reader = setup.reader(0);

  const WriteResult result = writer->write(message.t);

  ASSERT_EQ(WriteResult::Success, result);
  ASSERT_EQ(1, writer->message_count());

  const span<uint8_t> read = reader->next();

  ASSERT_EQ(1, reader->message_count());
  assert_eq(read, message.t);
}
}  // namespace

TEST(BlockingDemuxWriterTest, WriteRead1) {
  rc::check(write_and_read_1<true>);
}

TEST(NonBlockingDemuxWriterTest, WriteRead1) {
  rc::check(write_and_read_1<false>);
}

namespace {
template <bool Blocking>
auto one_reader_read_x(const vector<TestMessage>& valid_messages) {
  if (valid_messages.empty()) {
    return;
  }

  const size_t message_num = valid_messages.size();

  DemuxSetup<L, M, Blocking> setup{1};
  DemuxWriter<L, M, Blocking>* writer = setup.writer();
  DemuxReader<L, M>* reader = setup.reader(0);

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
}  // namespace

TEST(BlockingDemuxWriterTest, OneReaderReadX) {
  rc::check(one_reader_read_x<true>);
}

TEST(NonBlockingDemuxWriterTest, OneReaderReadX) {
  rc::check(one_reader_read_x<false>);
}

namespace {
template <bool Blocking>
auto multiple_readers_read_x(const vector<TestMessage>& valid_messages) -> void {
  if (valid_messages.empty()) {
    return;
  }

  constexpr uint8_t READER_NUM = 7;

  const size_t message_num = valid_messages.size();

  DemuxSetup<L, M, Blocking> setup{READER_NUM};
  DemuxWriter<L, M, Blocking>* writer = setup.writer();

  std::future<size_t> future_pub_result =
      std::async(std::launch::async, [&valid_messages, &writer] { return write_all(valid_messages, writer); });

  vector<std::future<vector<TestMessage>>> future_sub_results{};
  future_sub_results.reserve(READER_NUM);
  for (size_t i = 0; i < READER_NUM; i++) {
    DemuxReader<L, M>* reader = setup.reader(i);
    future_sub_results.emplace_back(std::async(std::launch::async, [message_num, reader] {
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
}  // namespace

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

namespace {
template <bool Blocking>
auto writer_lagging_readers(const vector<ReaderId>& readers) {
  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  DemuxWriter<L, M, Blocking> writer(0, span{buffer}, &msg_counter_sync, &wraparound_sync);

  ASSERT_TRUE(writer.lagging_readers().empty());

  for (auto reader : readers) {
    writer.add_reader(reader);
  }

  const std::set<ReaderId> expected(readers.begin(), readers.end());

  const std::vector<ReaderId> xs = writer.lagging_readers();
  const std::set<ReaderId> actual(xs.begin(), xs.end());

  ASSERT_EQ(expected, actual);

  for (auto reader : readers) {
    writer.remove_reader(reader);
  }

  ASSERT_TRUE(writer.lagging_readers().empty());
}
}  // namespace

namespace {
auto fill_up_buffer(DemuxWriter<L, M, false>* writer, TestMessage message) -> bool {
  while (true) {
    switch (writer->write(message.t)) {
      case WriteResult::Success:
        break;  // continue
      case WriteResult::Repeat:
        return true;  // buffer is full
      case WriteResult::Error:
        return false;  // fail test, should not happen
    }
  }
}

auto read_all_expect_eq(DemuxReader<L, M>* reader, TestMessage expected) -> bool {
  while (true) {
    const span<uint8_t> read = reader->next();
    if (read.empty()) {
      return true;
    }
    if (!expect_eq(expected.t, read)) {
      return false;
    }
  }
}

auto slow_reader_test(TestMessage message) -> bool {
  DemuxSetup<L, M, false> setup{1};
  DemuxWriter<L, M, false>* writer = setup.writer();
  DemuxReader<L, M>* reader = setup.reader(0);

  fill_up_buffer(writer, message);

  // the buffer is full, can't write into it
  EXPECT_EQ(WriteResult::Repeat, writer->write(message.t));

  read_all_expect_eq(reader, message);

  // all readers caught up, can write again
  EXPECT_EQ(WriteResult::Success, writer->write(message.t));

  return !::testing::Test::HasFailure();
}
}  // namespace

TEST(NonBlockingDemuxWriterTest, SlowReader) {
  rc::check(slow_reader_test);
}

auto main(int argc, char** argv) -> int {
  namespace logging = boost::log;
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::warning);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)