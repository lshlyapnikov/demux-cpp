// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner)

#define UNIT_TEST
#undef NDEBUG  // for assert to work in release build

#include <gtest/gtest.h>
#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <rapidcheck/Check.h>
#include <array>
#include <atomic>
#include <boost/log/core.hpp>         // NOLINT(misc-include-cleaner)
#include <boost/log/expressions.hpp>  // NOLINT(misc-include-cleaner)
#include <boost/log/trivial.hpp>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <limits>
#include <rapidcheck/gen/Arbitrary.hpp>
#include <span>
#include <vector>
#include "../core/demux_reader.h"
#include "../core/demux_writer.h"
#include "../core/message_buffer.h"
#include "../core/reader_id.h"
#include "../util/operators.h"
#include "./reader_id_gen.h"
#include "./test_message.h"

namespace lshl::demux::core {

// explicitly reference the generator to make clangd happy
using _ = rc::Arbitrary<lshl::demux::core::ReaderId>;

constexpr std::chrono::seconds DEFAULT_WAIT(5);
}  // namespace lshl::demux::core

using lshl::demux::core::DEFAULT_WAIT;
using lshl::demux::core::DemuxReader;
using lshl::demux::core::DemuxWriter;
using lshl::demux::core::ReaderId;
using lshl::demux::core::WriteResult;
using lshl::demux::core::test::TestMessage;

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
template <size_t L, uint16_t M, bool B>
class DemuxSetup {
 private:
  array<uint8_t, L> buffer_{};
  atomic<uint64_t> writer_sequence_{0};
  vector<atomic<size_t>> reader_positions_;
  vector<DemuxReader<L, M>> readers_;
  DemuxWriter<L, M, B> writer_;

 public:
  explicit DemuxSetup(const uint8_t reader_num)
      : reader_positions_(reader_num), writer_{span{buffer_}, &writer_sequence_, to_ptrs(reader_positions_)} {
    this->readers_.reserve(reader_num);
    for (uint8_t i = 0; i < reader_num; ++i) {
      atomic<size_t>* reader_position = &reader_positions_.at(i);
      assert(0 == reader_position->load());
      readers_.emplace_back(ReaderId{i}, this->buffer_, &this->writer_sequence_, &this->reader_positions_[i]);
    }
  }

  auto writer() -> DemuxWriter<L, M, B>* { return &writer_; }

  auto reader(const size_t index) -> DemuxReader<L, M>* { return &readers_.at(index); }

 private:
  static auto to_ptrs(vector<atomic<size_t>>& reader_positions) -> vector<atomic<size_t>*> {
    vector<atomic<size_t>*> result;
    result.reserve(reader_positions.size());
    for (auto& x : reader_positions) {
      result.push_back(&x);
    }
    return result;
  }
};

auto expect_eq(const span<const uint8_t>& left, const span<const uint8_t>& right) -> bool {
  EXPECT_EQ(left.size(), right.size());
  for (size_t i = 0; i < right.size(); ++i) {
    EXPECT_EQ(left[i], right[i]) << "index: " << i;
    if (::testing::Test::HasFailure()) {
      return false;
    }
  }
  return !::testing::Test::HasFailure();
}

auto expect_eq(const vector<TestMessage>& left, const vector<TestMessage>& right) -> bool {
  EXPECT_EQ(left.size(), right.size());
  for (size_t i = 0; i < right.size(); ++i) {
    const TestMessage& x = left[i];
    const TestMessage& y = right[i];
    using lshl::demux::core::test::operator<<;
    EXPECT_EQ(x, y) << "index: " << i;
    if (::testing::Test::HasFailure()) {
      return false;
    }
  }
  return !::testing::Test::HasFailure();
}

template <size_t L, uint16_t M, bool B>
auto write_all(const vector<TestMessage>& messages, DemuxWriter<L, M, B>* writer) -> size_t {
  size_t result = 0;
  for (size_t i = 0; i < messages.size();) {
    TestMessage m = messages[i];
    assert(m.t.size() > 0);
    assert(m.t.size() <= M);
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

template <size_t L, uint16_t M, bool B>
auto read_n(const size_t message_num, DemuxReader<L, M>* reader) -> vector<TestMessage> {
  vector<TestMessage> result;
  while (result.size() < message_num) {
    const span<const uint8_t>& m = reader->next();
    if (!m.empty()) {
      result.emplace_back(TestMessage(vector<uint8_t>{m.begin(), m.end()}));
    }
  }

  // read one more to unblock the reader, which might be waiting for the wraparound unblock
  const span<const uint8_t>& m = reader->next();
  assert(m.empty());

  return result;
}

}  // namespace

TEST(MultiplexerTest, Atomic) {
  EXPECT_EQ(std::atomic<uint8_t>{}.is_lock_free(), true);
  EXPECT_EQ(std::atomic<uint16_t>{}.is_lock_free(), true);
  EXPECT_EQ(std::atomic<uint32_t>{}.is_lock_free(), true);
  EXPECT_EQ(std::atomic<size_t>{}.is_lock_free(), true);
  EXPECT_EQ(std::atomic<uint64_t>{}.is_lock_free(), true);
  EXPECT_EQ(sizeof(size_t), sizeof(uint64_t));
}

namespace {
template <bool Blocking>
auto writer_constructor_does_not_throw(const uint8_t reader_num) -> void {
  if (reader_num == 0) {
    return;
  }
  DemuxSetup<L, M, Blocking> setup(reader_num);
  EXPECT_EQ(0, setup.writer()->sequence());
  for (uint8_t i = 0; i < reader_num; ++i) {
    EXPECT_EQ(0, setup.reader(i)->sequence());
    EXPECT_EQ(0, setup.reader(i)->position());
    EXPECT_EQ(ReaderId{i}, setup.reader(i)->id());
  }
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
  DemuxSetup<L, M, Blocking> setup(2);
  auto& writer = *(setup.writer());
  auto& reader0 = *(setup.reader(0));
  auto& reader1 = *(setup.reader(1));

  // write an empty message
  const WriteResult result = writer.write({});

  EXPECT_EQ(WriteResult::Error, result);
  EXPECT_EQ(0, writer.sequence());

  const span<const uint8_t> msg0 = reader0.next();
  EXPECT_EQ(0, msg0.size());
  EXPECT_EQ(0, reader0.sequence());

  const span<const uint8_t> msg1 = reader1.next();
  EXPECT_EQ(0, msg1.size());
  EXPECT_EQ(0, reader1.sequence());
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
  DemuxSetup<L, M, Blocking> setup(1);
  auto& writer = *(setup.writer());
  auto& reader = *(setup.reader(0));

  array<uint8_t, L> m{1};  // this should not fit into the buffer given M + 2 requirement
  const WriteResult result = writer.write(m);
  EXPECT_EQ(WriteResult::Error, result);
  EXPECT_EQ(0, writer.sequence());

  const span<const uint8_t> read = reader.next();
  EXPECT_EQ(0, read.size());
  EXPECT_EQ(0, reader.sequence());
}
}  // namespace

TEST(BlockingDemuxWriterTest, WriteInvalidLargeMessage) {
  write_invalid_large_message<true>();
}

TEST(NonBlockingDemuxWriterTest, WriteInvalidLargeMessage) {
  write_invalid_large_message<false>();
}

TEST(NonBlockingDemuxWriterTest, WriteWhenBufferIfFullAndGetWriteRepeatResult) {
  DemuxSetup<L, M, false> setup(1);
  auto* writer = setup.writer();
  auto* reader = setup.reader(0);

  EXPECT_EQ(L, 2 * M);

  array<uint8_t, M> m1{1};
  EXPECT_EQ(WriteResult::Success, writer->write(m1));
  EXPECT_EQ(1, writer->sequence());

  array<uint8_t, M> m2{2};
  EXPECT_EQ(WriteResult::Repeat, writer->write(m2));
  EXPECT_EQ(1, writer->sequence());  // empty message isn't written yet

  expect_eq(m1, reader->next());
  EXPECT_EQ(1, reader->sequence());

  EXPECT_TRUE(reader->next().empty());
  EXPECT_EQ(1, reader->sequence());

  EXPECT_EQ(WriteResult::Repeat, writer->write(m2));
  EXPECT_EQ(2, writer->sequence());  // empty message is written

  EXPECT_EQ(WriteResult::Repeat, writer->write(m2));
  EXPECT_EQ(2, writer->sequence());  // reader hasn't moved yet

  EXPECT_TRUE(reader->next().empty());
  EXPECT_EQ(2, reader->sequence());  // empty message is read

  EXPECT_EQ(WriteResult::Success, writer->write(m2));
  EXPECT_EQ(3, writer->sequence());

  expect_eq(m2, reader->next());
  EXPECT_EQ(3, reader->sequence());

  array<uint8_t, M> m3{3};
  EXPECT_EQ(WriteResult::Repeat, writer->write(m3));
  EXPECT_EQ(4, writer->sequence());  // empty message written

  EXPECT_TRUE(reader->next().empty());
  EXPECT_EQ(4, reader->sequence());

  EXPECT_EQ(WriteResult::Success, writer->write(m3));
  EXPECT_EQ(5, writer->sequence());  // empty message written

  expect_eq(m3, reader->next());
  EXPECT_EQ(5, reader->sequence());

  EXPECT_TRUE(reader->next().empty());
  EXPECT_EQ(5, reader->sequence());
}

namespace {
template <bool Blocking>
auto write_and_read_1(TestMessage message) {
  if (message.t.size() > M) {
    return;
  }
  DemuxSetup<L, M, false> setup(1);
  auto* writer = setup.writer();
  auto* reader = setup.reader(0);

  const WriteResult result = writer->write(message.t);

  EXPECT_EQ(WriteResult::Success, result);
  EXPECT_EQ(1, writer->sequence());

  const span<const uint8_t> read = reader->next();

  EXPECT_EQ(1, reader->sequence());
  expect_eq(read, message.t);
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
auto one_reader_read_x(const vector<TestMessage>& messages) {
  if (messages.empty()) {
    return;
  }

  using lshl::demux::util::operator<<;

  std::string str;
  for (const auto& msg : messages) {
    str += std::to_string(msg.t.size()) + ", ";
  }

  LOG_INFO << "message number: " << messages.size();
  LOG_INFO << "message lengths: " << str;
  LOG_INFO << "messages: " << messages;

  const size_t message_num = messages.size();

  DemuxSetup<L, M, false> setup(1);
  auto* writer = setup.writer();
  auto* reader = setup.reader(0);

  std::future<size_t> sent_count_future =
      std::async(std::launch::async, [&messages, &writer] { return write_all(messages, writer); });

  std::future<vector<TestMessage>> received_messages_future =
      std::async(std::launch::async, [message_num, &reader] { return read_n<L, M, Blocking>(message_num, reader); });

  sent_count_future.wait_for(DEFAULT_WAIT);
  EXPECT_TRUE(sent_count_future.valid());

  const size_t sent_count = sent_count_future.get();
  EXPECT_EQ(message_num, sent_count);

  received_messages_future.wait_for(DEFAULT_WAIT);
  EXPECT_TRUE(received_messages_future.valid());

  const auto received_messages = received_messages_future.get();
  expect_eq(messages, received_messages);
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

  DemuxSetup<L, M, false> setup(READER_NUM);
  DemuxWriter<L, M, false>* writer = setup.writer();

  std::future<size_t> future_pub_result =
      std::async(std::launch::async, [&valid_messages, &writer] { return write_all(valid_messages, writer); });

  vector<std::future<vector<TestMessage>>> future_sub_results{};
  future_sub_results.reserve(READER_NUM);
  for (uint8_t i = 0; i < READER_NUM; ++i) {
    DemuxReader<L, M>* reader = setup.reader(i);
    future_sub_results.emplace_back(std::async(std::launch::async, [message_num, reader] {
      return read_n<L, M, Blocking>(message_num, reader);
    }));
  }

  future_pub_result.wait_for(DEFAULT_WAIT);
  EXPECT_TRUE(future_pub_result.valid());
  EXPECT_EQ(message_num, future_pub_result.get());

  for (auto& future_sub_result : future_sub_results) {
    future_sub_result.wait_for(DEFAULT_WAIT);
    EXPECT_TRUE(future_sub_result.valid());
    expect_eq(valid_messages, future_sub_result.get());
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
    const span<const uint8_t> read = reader->next();
    if (read.empty()) {
      return true;
    }
    if (!expect_eq(expected.t, read)) {
      return false;
    }
  }
}

auto slow_reader_test(TestMessage message) -> bool {
  DemuxSetup<L, M, false> setup(1);

  fill_up_buffer(setup.writer(), message);

  // the buffer is full, can't write into it
  EXPECT_EQ(WriteResult::Repeat, setup.writer()->write(message.t));

  read_all_expect_eq(setup.reader(0), message);

  // all readers caught up, can write again
  EXPECT_EQ(WriteResult::Success, setup.writer()->write(message.t));

  return !::testing::Test::HasFailure();
}
}  // namespace

TEST(NonBlockingDemuxWriterTest, SlowReader) {
  rc::check(slow_reader_test);
}

auto main(int argc, char** argv) -> int {
  namespace logging = boost::log;
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)