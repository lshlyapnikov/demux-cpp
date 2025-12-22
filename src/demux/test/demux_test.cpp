// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

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
#include <memory>
#include <rapidcheck/gen/Arbitrary.hpp>
#include <span>
#include <vector>
#include "../core/demux_reader.h"
#include "../core/demux_writer.h"
#include "../core/message_buffer.h"
#include "../core/reader_id.h"
#include "../example/market_event.h"
#include "../util/operators.h"
#include "./demux_setup.h"
#include "./market_event_gen.h"
#include "./reader_id_gen.h"
#include "./test_message.h"
namespace {

// explicitly reference the generator to make clangd happy
using _0 = rc::Arbitrary<lshl::demux::core::ReaderId>;
using _1 = rc::Arbitrary<lshl::demux::core::test::TestMessage>;
using _2 = rc::Arbitrary<lshl::demux::example::MarketEvent>;

constexpr std::chrono::seconds DEFAULT_WAIT(5);

using lshl::demux::core::DemuxReader;
using lshl::demux::core::DemuxWriter;
using lshl::demux::core::ReaderId;
using lshl::demux::example::MarketDataUpdate;
using lshl::demux::example::MarketEvent;
using lshl::demux::example::MarketTradeUpdate;

using lshl::demux::core::test::DemuxSetup;
using lshl::demux::core::test::L;
using lshl::demux::core::test::M;
using lshl::demux::core::test::TestMessage;

using std::array;
using std::atomic;
using std::span;
using std::uint16_t;
using std::uint8_t;
using std::vector;

// [[nodiscard]] auto expect_eq(const span<const uint8_t>& left, const span<const uint8_t>& right) -> bool {
//   EXPECT_EQ(left.size(), right.size());
//   for (size_t i = 0; i < right.size(); ++i) {
//     EXPECT_EQ(left[i], right[i]) << "index: " << i;
//     if (::testing::Test::HasFailure()) {
//       return false;
//     }
//   }
//   return !::testing::Test::HasFailure();
// }

// auto assert_eq(const vector<TestMessage>& left, const vector<TestMessage>& right) -> void {
//   ASSERT_EQ(left.size(), right.size());
//   for (size_t i = 0; i < right.size(); ++i) {
//     const TestMessage& x = left[i];
//     const TestMessage& y = right[i];
//     using lshl::demux::core::test::operator<<;
//     ASSERT_EQ(x, y) << "index: " << i << ", x: " << x << ", y: " << y;
//   }
// }

template <typename M, size_t N, bool B>
[[nodiscard]] auto write_all(const vector<M>& messages, DemuxWriter<M, N, B>* writer) -> size_t {
  size_t result = 0;

  for (size_t i = 0; i < messages.size(); i++) {
    const M& m = messages[i];
    while (!writer->emplace(m)) {
      // busy-wait
    }
    EXPECT_TRUE(writer->commit());
    if (::testing::Test::HasFailure()) {
      return result;
    }
    result += 1;
  }

  EXPECT_EQ(messages.size(), result) << "writer: " << *writer;
  EXPECT_EQ(messages.size() % N, writer->tail()) << "writer: " << *writer;
  if (::testing::Test::HasFailure()) {
    return 0;
  }

  return result;
}

template <typename M, size_t N, bool B>
[[nodiscard]] auto read_n(const size_t message_num, DemuxReader<M, N, B>* reader) -> vector<M> {
  vector<M> result;
  result.reserve(message_num);
  while (result.size() < message_num) {
    const std::optional<const M*> ptr = reader->next();
    if (ptr.has_value()) {
      const M* m = ptr.value();
      result.emplace_back(*m);
    }
  }

  // assert no more messages are available if non-blocking reader, else it will block
  if constexpr (!B) {
    EXPECT_FALSE(reader->next().has_value()) << "reader: " << *reader;
  }

  if (::testing::Test::HasFailure()) {
    return vector<M>{};
  }

  return result;
}

template <bool Blocking>
auto writer_constructor_does_not_throw(const uint8_t reader_num) -> void {
  if (reader_num == 0) {
    ASSERT_THROW((DemuxSetup<MarketDataUpdate, 16, Blocking>(reader_num)), std::invalid_argument);
  } else if (reader_num > 64) {
    ASSERT_THROW((DemuxSetup<MarketDataUpdate, 16, Blocking>(reader_num)), std::invalid_argument);
  } else {
    DemuxSetup<MarketDataUpdate, 16, Blocking> setup(reader_num);
    ASSERT_EQ(0, setup.writer()->tail());
    for (uint8_t i = 0; i < reader_num; ++i) {
      ASSERT_EQ(true, setup.is_active_reader(i));
      ASSERT_EQ(0, setup.reader(i)->tail());
      ASSERT_EQ(0, setup.reader(i)->head());
      ASSERT_EQ(ReaderId{i}, setup.reader(i)->id());
    }
  }

  ASSERT_FALSE(::testing::Test::HasFailure());
}
}  // namespace

TEST(BlockingDemuxTest, ConstructorDoesNotThrow) {
  rc::check(writer_constructor_does_not_throw<true>);
}

TEST(NonBlockingDemuxTest, ConstructorDoesNotThrow) {
  rc::check(writer_constructor_does_not_throw<false>);
}

TEST(DemuxTest, Atomic) {
  ASSERT_EQ(std::atomic<uint8_t>{}.is_lock_free(), true);
  ASSERT_EQ(std::atomic<uint16_t>{}.is_lock_free(), true);
  ASSERT_EQ(std::atomic<uint32_t>{}.is_lock_free(), true);
  ASSERT_EQ(std::atomic<size_t>{}.is_lock_free(), true);
  ASSERT_EQ(std::atomic<uint64_t>{}.is_lock_free(), true);
  ASSERT_EQ(sizeof(size_t), sizeof(uint64_t));
}

namespace {
template <bool Blocking>
auto write_and_read_1(MarketEvent message) {
  DemuxSetup<MarketEvent, 2, Blocking> setup(1);
  auto* writer = setup.writer();
  auto* reader = setup.reader(0);

  ASSERT_TRUE(writer->emplace(message)) << "writer: " << *writer;
  ASSERT_EQ(0, writer->tail()) << "writer: " << *writer;
  ASSERT_TRUE(writer->commit()) << "writer: " << *writer;

  ASSERT_EQ(1, writer->tail()) << "writer: " << *writer;
  ASSERT_EQ(0, reader->head()) << "reader: " << *reader;

  const std::optional<const MarketEvent*> read = reader->next();
  ASSERT_TRUE(read.has_value()) << "reader: " << *reader;
  ASSERT_EQ(message, *(read.value())) << "reader: " << *reader;

  ASSERT_EQ(1, writer->tail()) << "writer: " << *writer;
  ASSERT_EQ(1, reader->head()) << "reader: " << *reader;

  ASSERT_FALSE(::testing::Test::HasFailure());
}
}  // namespace

TEST(BlockingDemuxTest, WriteRead1) {
  rc::check(write_and_read_1<true>);
}

TEST(NonBlockingDemuxTest, WriteRead1) {
  rc::check(write_and_read_1<false>);
}

namespace {
template <size_t N>
auto nonBlockingWriteWhenBufferIsFull(array<MarketEvent, N> events) -> void {
  ASSERT_EQ(N, events.size());

  DemuxSetup<MarketEvent, N, false> setup(2);
  auto* writer = setup.writer();
  auto* reader0 = setup.reader(0);
  auto* reader1 = setup.reader(1);

  ASSERT_EQ(0, writer->tail());
  ASSERT_EQ(0, reader0->tail());
  ASSERT_EQ(0, reader1->tail());
  ASSERT_EQ(0, reader0->head());
  ASSERT_EQ(0, reader1->head());

  // write N - 1 messages;
  // you can't write the last element and wrap around if at least one reader is at position 0.
  for (size_t i = 0; i < N - 1; ++i) {
    std::optional<MarketEvent*> ptr = writer->next();
    ASSERT_TRUE(ptr.has_value()) << "i: " << i << ", writer: " << *writer;
    *(ptr.value()) = events.at(i);

    ASSERT_EQ(i, writer->tail());
    ASSERT_TRUE(writer->commit());
    ASSERT_EQ(i + 1, writer->tail());
  }

  ASSERT_EQ(N - 1, writer->tail());
  ASSERT_EQ(N - 1, reader0->tail());
  ASSERT_EQ(N - 1, reader1->tail());
  ASSERT_EQ(0, reader0->head());
  ASSERT_EQ(0, reader1->head());

  // readers read the first message at position 0.
  for (size_t r = 0; r < 2; ++r) {
    DemuxReader<MarketEvent, N, false>* reader = setup.reader(r);
    const std::optional<const MarketEvent*> read = reader->next();
    ASSERT_TRUE(read.has_value());
    const MarketEvent& expected = events.at(0);
    const MarketEvent& actual = *(read.value());
    ASSERT_EQ(expected, actual) << "r: " << r << ", reader: " << reader;
  }

  ASSERT_EQ(N - 1, writer->tail());
  ASSERT_EQ(N - 1, reader0->tail());
  ASSERT_EQ(N - 1, reader1->tail());
  ASSERT_EQ(1, reader0->head());
  ASSERT_EQ(1, reader1->head());

  // now writer can write the last message and wrap around.
  {
    std::optional<MarketEvent*> ptr = writer->next();
    ASSERT_TRUE(ptr.has_value()) << "writer: " << *writer;
    *(ptr.value()) = events.at(N - 1);
    ASSERT_TRUE(writer->commit());
  }

  ASSERT_EQ(0, writer->tail());
  ASSERT_EQ(0, reader0->tail());
  ASSERT_EQ(0, reader1->tail());
  ASSERT_EQ(1, reader0->head());
  ASSERT_EQ(1, reader1->head());

  // buffer is full, can't write any more.
  ASSERT_FALSE(writer->next().has_value());

  ASSERT_EQ(0, writer->tail());
  ASSERT_EQ(0, reader0->tail());
  ASSERT_EQ(0, reader1->tail());
  ASSERT_EQ(1, reader0->head());
  ASSERT_EQ(1, reader1->head());

  // readers read remaining messages, starting from index 1; the first message at position 0, has already been read.
  for (size_t i = 1; i < N; ++i) {
    for (size_t r = 0; r < 2; ++r) {
      DemuxReader<MarketEvent, N, false>* reader = setup.reader(r);
      const std::optional<const MarketEvent*> read = reader->next();
      ASSERT_TRUE(read.has_value());
      const MarketEvent& expected = events.at(i);
      const MarketEvent& actual = *(read.value());
      ASSERT_EQ(expected, actual) << "i: " << i << ", r: " << r << ", reader: " << reader;
      ASSERT_EQ(0, reader->tail());
      ASSERT_EQ((i + 1) % N, reader->head());
    }
  }

  ASSERT_EQ(0, writer->tail());
  ASSERT_EQ(0, reader0->tail());
  ASSERT_EQ(0, reader1->tail());
  ASSERT_EQ(0, reader0->head());
  ASSERT_EQ(0, reader1->head());

  // can write again after readers have read all messages
  {
    std::optional<MarketEvent*> ptr = writer->next();
    ASSERT_TRUE(ptr.has_value());
    *(ptr.value()) = events.at(0);
    ASSERT_TRUE(writer->commit());
  }

  ASSERT_EQ(1, writer->tail());
  ASSERT_EQ(1, reader0->tail());
  ASSERT_EQ(1, reader1->tail());
  ASSERT_EQ(0, reader0->head());
  ASSERT_EQ(0, reader1->head());

  ASSERT_FALSE(::testing::Test::HasFailure());
}
}  // namespace

TEST(NonBlockingDemuxTest, WriteWhenBufferIsFull2) {
  rc::check(nonBlockingWriteWhenBufferIsFull<2>);
}

TEST(NonBlockingDemuxTest, WriteWhenBufferIsFull4) {
  rc::check(nonBlockingWriteWhenBufferIsFull<4>);
}

TEST(NonBlockingDemuxTest, WriteWhenBufferIsFull8) {
  rc::check(nonBlockingWriteWhenBufferIsFull<8>);
}

TEST(NonBlockingDemuxTest, WriteWhenBufferIsFull16) {
  rc::check(nonBlockingWriteWhenBufferIsFull<16>);
}

namespace {
template <typename M, size_t N, bool B, size_t ReaderNum>
auto multiple_readers(const vector<M>& messages) -> void {
  if (messages.empty()) {
    return;
  }

  const size_t message_num = messages.size();

  DemuxSetup<M, N, B> setup(ReaderNum);
  DemuxWriter<M, N, B>* writer = setup.writer();

  std::future<size_t> future_writer_result =
      std::async(std::launch::async, [&messages, &writer] { return write_all(messages, writer); });

  vector<std::future<vector<M>>> future_reader_results{};
  future_reader_results.reserve(ReaderNum);
  for (size_t i = 0; i < ReaderNum; ++i) {
    auto* reader = setup.reader(i);
    future_reader_results.emplace_back(std::async(std::launch::async, [message_num, reader] {
      return read_n(message_num, reader);
    }));
  }

  future_writer_result.wait_for(DEFAULT_WAIT);
  ASSERT_TRUE(future_writer_result.valid());
  ASSERT_EQ(message_num, future_writer_result.get());

  for (auto& x : future_reader_results) {
    x.wait_for(DEFAULT_WAIT);
    ASSERT_TRUE(x.valid());
    ASSERT_EQ(messages, x.get());
  }

  ASSERT_FALSE(::testing::Test::HasFailure());
}
}  // namespace

TEST(NonBlockingDemuxTest, MultipleReaders1) {
  rc::check(multiple_readers<MarketEvent, 16, false, 1>);
}

TEST(NonBlockingDemuxTest, MultipleReaders2) {
  rc::check(multiple_readers<MarketEvent, 16, false, 2>);
}

TEST(NonBlockingDemuxTest, MultipleReaders7) {
  rc::check(multiple_readers<MarketEvent, 16, false, 7>);
}

TEST(BlockingDemuxTest, MultipleReaders1) {
  rc::check(multiple_readers<MarketEvent, 16, true, 1>);
}

TEST(BlockingDemuxTest, MultipleReaders2) {
  rc::check(multiple_readers<MarketEvent, 16, true, 2>);
}

TEST(BlockingDemuxTest, MultipleReaders7) {
  rc::check(multiple_readers<MarketEvent, 16, true, 7>);
}

/*
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
[[nodiscard]] auto fill_up_buffer(DemuxWriter<L, M, false>* writer, TestMessage message) -> size_t {
  size_t result = 0;
  while (true) {
    switch (writer->write(message.t)) {
      case WriteResult::Success:
        result += 1;
        continue;
      case WriteResult::Repeat:
        return result;  // the buffer is full
      case WriteResult::Error:
        LOG_ERROR << "Unexpected WriteResult::Error when writing message, number of messages written: " << result;
        return 0;
    }
  }
}

auto read_all_assert_eq(DemuxReader<L, M>* reader, TestMessage expected) -> void {
  while (true) {
    const span<const uint8_t> read = reader->next();
    if (read.empty()) {
      break;
    }
    ASSERT_TRUE(expect_eq(expected.t, read));
  }
}

TEST(NonBlockingDemuxTest, Wraparound) {
  ASSERT_EQ(L, 2 * M);

  DemuxSetup<L, M, false> setup(1);
  DemuxWriter<L, M, false>* writer = setup.writer();
  DemuxReader<L, M>* reader = setup.reader(0);

  array<uint8_t, M> m1{1};
  ASSERT_EQ(WriteResult::Success, writer->write(m1));
  ASSERT_EQ(1, writer->sequence());

  array<uint8_t, M> m2{2};
  ASSERT_EQ(WriteResult::Repeat, writer->write(m2));
  ASSERT_EQ(1, writer->sequence());  // empty message isn't written yet, reader hasn't moved

  ASSERT_EQ(WriteResult::Repeat, writer->write(m2));
  ASSERT_EQ(1, writer->sequence());  // empty message isn't written yet, reader hasn't moved

  ASSERT_TRUE(expect_eq(m1, reader->next()));
  ASSERT_EQ(1, reader->sequence());

  ASSERT_EQ(WriteResult::Repeat, writer->write(m2));
  ASSERT_EQ(2, writer->sequence());  // empty message is written, reader moved

  ASSERT_TRUE(expect_eq({}, reader->next()));
  ASSERT_EQ(2, reader->sequence());

  ASSERT_EQ(WriteResult::Success, writer->write(m2));
  ASSERT_EQ(3, writer->sequence());

  ASSERT_TRUE(expect_eq(m2, reader->next()));
  ASSERT_EQ(3, reader->sequence());
}

auto slow_reader(TestMessage message) -> void {
  LOG_DEBUG << "---------------";
  LOG_DEBUG << "message: " << message << ", size: " << message.t.size();

  DemuxSetup<L, M, false> setup(1);
  auto* writer = setup.writer();
  auto* reader = setup.reader(0);

  const size_t messages_to_fill_buffer = (L / (message.t.size() + sizeof(lshl::demux::core::message_length_t)));

  ASSERT_EQ(messages_to_fill_buffer, fill_up_buffer(writer, message));
  ASSERT_EQ(messages_to_fill_buffer, writer->sequence());

  // the buffer is full, can't write anymore messages
  ASSERT_EQ(WriteResult::Repeat, writer->write(message.t));
  ASSERT_EQ(messages_to_fill_buffer, writer->sequence());

  // read all messages, to catup with the writer
  read_all_assert_eq(reader, message);
  ASSERT_EQ(writer->sequence(), reader->sequence());

  // make sure no more messages are available to read
  ASSERT_EQ(reader->next().empty(), true);
  ASSERT_EQ(writer->sequence(), reader->sequence());

  // wraparound marker is written at this point
  switch (writer->write(message.t)) {
    case WriteResult::Success:
      // wraparound + 1 message written
      ASSERT_EQ(messages_to_fill_buffer + 2, writer->sequence());
      break;
    case WriteResult::Repeat:
      // only wraparound marker is written
      ASSERT_EQ(messages_to_fill_buffer + 1, writer->sequence());
      // read the wraparound marker
      ASSERT_TRUE(reader->next().empty());
      ASSERT_EQ(writer->sequence(), reader->sequence());
      // no we should be able to write the message
      ASSERT_EQ(WriteResult::Success, writer->write(message.t));
      ASSERT_EQ(messages_to_fill_buffer + 2, writer->sequence());
      break;
    case WriteResult::Error:
      FAIL() << "Unexpected WriteResult::Error when writing message after reading all messages: " << message;
  }

  read_all_assert_eq(reader, message);
  ASSERT_TRUE(reader->next().empty());

  ASSERT_EQ(writer->sequence(), reader->sequence());
  // ASSERT_EQ(reader->next().empty(), true);
  // ASSERT_EQ(writer->sequence(), reader->sequence());

  // ASSERT_EQ(WriteResult::Success, writer->write(message.t));
}
}  // namespace

TEST(NonBlockingDemuxTest, SlowReader) {
  rc::check(slow_reader);
}

TEST(NonBlockingDemuxTest, SlowReader1) {
  vector<uint8_t> m{0xde, 0x4a, 0xf5, 0x86, 0xd1, 0xea, 0xf7, 0x55, 0xa0, 0xc0, 0xcf, 0x07, 0x48, 0xbf, 0x37, 0xc5,
                    0x6a, 0xb7, 0xbc, 0x01, 0x92, 0xa0, 0xea, 0xdd, 0x93, 0x6d, 0xc3, 0x62, 0xa3, 0xba, 0x97, 0xe2,
                    0x68, 0x04, 0x74, 0xe0, 0x9e, 0x32, 0x04, 0xc3, 0x3e, 0x8b, 0xfd, 0xcd, 0x3b, 0x4e, 0x21, 0x7e,
                    0xca, 0x2b, 0x07, 0xdd, 0x19, 0x4d, 0x76, 0x0d, 0xb7, 0xb7, 0x1c, 0x7a, 0x54, 0x71, 0x57, 0x38};
  ASSERT_EQ(m.size(), 64);
  slow_reader(TestMessage(m));
}

TEST(NonBlockingDemuxTest, SlowReader2) {
  vector<uint8_t> m{0x77, 0x2b, 0xdb, 0x58, 0xb9, 0x21, 0xca, 0xfe, 0xca, 0x64, 0xf6, 0x8e, 0x55, 0xc4,
                    0x06, 0x69, 0x83, 0x72, 0x05, 0x32, 0x61, 0x7e, 0x6b, 0x54, 0x56, 0x40, 0x2f, 0x4d,
                    0xaf, 0x0a, 0x1f, 0x72, 0x68, 0x85, 0x25, 0x35, 0x28, 0x54, 0x6d, 0xee, 0x89, 0x88,
                    0xab, 0x71, 0x8a, 0xae, 0x1d, 0x3b, 0xbf, 0x20, 0xab, 0x0f, 0x83, 0x6d, 0x24, 0x65};
  ASSERT_EQ(m.size(), 56);
  slow_reader(TestMessage(m));
}
*/

auto main(int argc, char** argv) -> int {
  namespace logging = boost::log;
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::warning);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)