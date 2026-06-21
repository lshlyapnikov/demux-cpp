// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

#define UNIT_TEST
#undef NDEBUG  // for assert to work in release build

#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/Check.h>
#include <array>
#include <atomic>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <rapidcheck/gen/Arbitrary.hpp>
#include <vector>
#include "../core/demux_reader.h"
#include "../core/demux_writer.h"
#include "../core/reader_id.h"
#include "../example/market_event.h"
#include "./demux_setup.h"
#include "./market_event_gen.h"

namespace {

// explicitly reference the generator to make clangd happy
using _0 = rc::Arbitrary<lshl::demux::example::MarketEvent>;

constexpr std::chrono::seconds DEFAULT_WAIT(5);

using lshl::demux::core::DemuxReader;
using lshl::demux::core::DemuxWriter;
using lshl::demux::core::ReaderId;
using lshl::demux::core::test::DemuxSetup;
using lshl::demux::example::MarketDataUpdate;
using lshl::demux::example::MarketEvent;
using std::array;
using std::atomic;
using std::uint16_t;
using std::uint8_t;
using std::vector;

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
    const M* const ptr = reader->next();
    if (ptr) {
      const M* m = ptr;
      result.emplace_back(*m);
    }
  }

  // assert no more messages are available if non-blocking reader, else it will block
  if constexpr (!B) {
    EXPECT_EQ(nullptr, reader->next()) << "reader: " << *reader;
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

  const MarketEvent* read = reader->next();
  if (read) {
    ASSERT_EQ(message, *read) << "reader: " << *reader;
  } else {
    FAIL() << "expected value, but got none, reader: " << *reader;
  }

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
    MarketEvent* ptr = writer->next();
    if (ptr) {
      *ptr = events.at(i);
    } else {
      FAIL() << "expected value, but got none, i: " << i << ", writer: " << *writer;
    }
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
    const MarketEvent* read = reader->next();
    if (read) {
      const MarketEvent& expected = events.at(0);
      const MarketEvent& actual = *read;
      ASSERT_EQ(expected, actual) << "r: " << r << ", reader: " << reader;
    } else {
      FAIL() << "expected value, but got none, r: " << r << ", reader: " << reader;
    }
  }

  ASSERT_EQ(N - 1, writer->tail());
  ASSERT_EQ(N - 1, reader0->tail());
  ASSERT_EQ(N - 1, reader1->tail());
  ASSERT_EQ(1, reader0->head());
  ASSERT_EQ(1, reader1->head());

  // now writer can write the last message and wrap around.
  {
    MarketEvent* ptr = writer->next();
    if (ptr) {
      *ptr = events.at(N - 1);
    } else {
      FAIL() << "expected value, but got none, writer: " << *writer;
    }
    ASSERT_TRUE(writer->commit());
  }

  ASSERT_EQ(0, writer->tail());
  ASSERT_EQ(0, reader0->tail());
  ASSERT_EQ(0, reader1->tail());
  ASSERT_EQ(1, reader0->head());
  ASSERT_EQ(1, reader1->head());

  // buffer is full, can't write any more.
  ASSERT_EQ(nullptr, writer->next());

  ASSERT_EQ(0, writer->tail());
  ASSERT_EQ(0, reader0->tail());
  ASSERT_EQ(0, reader1->tail());
  ASSERT_EQ(1, reader0->head());
  ASSERT_EQ(1, reader1->head());

  // readers read remaining messages, starting from index 1; the first message at position 0, has already been read.
  for (size_t i = 1; i < N; ++i) {
    for (size_t r = 0; r < 2; ++r) {
      DemuxReader<MarketEvent, N, false>* reader = setup.reader(r);
      const MarketEvent* const read = reader->next();
      if (read) {
        const MarketEvent& expected = events.at(i);
        const MarketEvent& actual = *read;
        ASSERT_EQ(expected, actual) << "i: " << i << ", r: " << r << ", reader: " << reader;
      } else {
        FAIL() << "expected value, but got none, i: " << i << ", r: " << r << ", reader: " << reader;
      }
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
    MarketEvent* ptr = writer->next();
    if (ptr) {
      *ptr = events.at(0);
    } else {
      FAIL() << "expected value, but got none, writer: " << *writer;
    }
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
*/

auto main(int argc, char** argv) -> int {
  namespace logging = boost::log;
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::warning);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)