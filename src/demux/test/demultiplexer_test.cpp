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
#include "./reader_id_gen.h"
#include "./test_message.h"

namespace {

// explicitly reference the generator to make clangd happy
using _0 = rc::Arbitrary<lshl::demux::core::ReaderId>;
using _1 = rc::Arbitrary<lshl::demux::core::test::TestMessage>;

// constexpr std::chrono::seconds DEFAULT_WAIT(5);

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

// template <typename M, size_t N, bool B>
// [[nodiscard]] auto write_all(const vector<TestMessage>& messages, DemuxWriter<M, N, B>* writer) -> size_t {
//   size_t result = 0;
//   for (size_t i = 0; i < messages.size();) {
//     TestMessage m = messages[i];
//     assert(m.t.size() > 0);
//     assert(m.t.size() <= M);
//     switch (writer->write(m.t)) {
//       case WriteResult::Success:
//         i += 1;
//         result += 1;
//         break;  // break the swtich, not the while-loop in other words continue writing next message
//       case WriteResult::Repeat:
//         return result;
//       case WriteResult::Error:
//         LOG_ERROR << "Unexpected WriteResult::Error when writing message index " << i << ": " << m;
//         return result;
//     }
//   }
//   LOG_ERROR << "Should be unreachable";
//   return result;  // to suppress compiler warning
// }

// template <size_t L, uint16_t M, bool B>
// [[nodiscard]] auto read_n(const size_t message_num, DemuxReader<L, M>* reader) -> vector<TestMessage> {
//   vector<TestMessage> result;
//   while (result.size() < message_num) {
//     const span<const uint8_t>& m = reader->next();
//     if (!m.empty()) {
//       result.emplace_back(TestMessage(vector<uint8_t>{m.begin(), m.end()}));
//     }
//   }

//   // read one more to unblock the reader, which might be waiting for the wraparound unblock
//   const span<const uint8_t>& m = reader->next();
//   assert(m.empty());

//   return result;
// }

template <bool Blocking>
auto writer_constructor_does_not_throw(const uint8_t reader_num) -> void {
  if (reader_num == 0) {
    return;
  }
  DemuxSetup<MarketDataUpdate, 16, Blocking> setup(reader_num);
  ASSERT_EQ(0, setup.writer()->tail());
  for (uint8_t i = 0; i < reader_num; ++i) {
    ASSERT_EQ(0, setup.reader(i)->tail());
    ASSERT_EQ(0, setup.reader(i)->head());
    ASSERT_EQ(ReaderId{i}, setup.reader(i)->id());
  }
}
}  // namespace

TEST(BlockingDemuxTest, ConstructorDoesNotThrow) {
  rc::check(writer_constructor_does_not_throw<true>);
}

TEST(NonBlockingDemuxTest, ConstructorDoesNotThrow) {
  rc::check(writer_constructor_does_not_throw<false>);
}

/*
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
auto write_empty_message() {
  DemuxSetup<L, M, Blocking> setup(2);
  auto& writer = *(setup.writer());
  auto& reader0 = *(setup.reader(0));
  auto& reader1 = *(setup.reader(1));

  // write an empty message
  const WriteResult result = writer.write({});

  ASSERT_EQ(WriteResult::Error, result);
  ASSERT_EQ(0, writer.sequence());

  const span<const uint8_t> msg0 = reader0.next();
  ASSERT_EQ(0, msg0.size());
  ASSERT_EQ(0, reader0.sequence());

  const span<const uint8_t> msg1 = reader1.next();
  ASSERT_EQ(0, msg1.size());
  ASSERT_EQ(0, reader1.sequence());
}
}  // namespace

TEST(BlockingDemuxTest, WriteEmptyMessage) {
  write_empty_message<true>();
}

TEST(NonBlockingDemuxTest, WriteEmptyMessage) {
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
  ASSERT_EQ(WriteResult::Error, result);
  ASSERT_EQ(0, writer.sequence());

  const span<const uint8_t> read = reader.next();
  ASSERT_EQ(0, read.size());
  ASSERT_EQ(0, reader.sequence());
}
}  // namespace

TEST(BlockingDemuxTest, WriteInvalidLargeMessage) {
  write_invalid_large_message<true>();
}

TEST(NonBlockingDemuxTest, WriteInvalidLargeMessage) {
  write_invalid_large_message<false>();
}

TEST(NonBlockingDemuxTest, WriteWhenBufferIfFullAndGetWriteRepeatResult) {
  DemuxSetup<L, M, false> setup(1);
  auto* writer = setup.writer();
  auto* reader = setup.reader(0);

  ASSERT_EQ(L, 2 * M);

  array<uint8_t, M> m1{1};
  ASSERT_EQ(WriteResult::Success, writer->write(m1));
  ASSERT_EQ(1, writer->sequence());

  array<uint8_t, M> m2{2};
  ASSERT_EQ(WriteResult::Repeat, writer->write(m2));
  ASSERT_EQ(1, writer->sequence());  // empty message isn't written yet

  ASSERT_TRUE(expect_eq(m1, reader->next()));
  ASSERT_EQ(1, reader->sequence());

  ASSERT_TRUE(reader->next().empty());
  ASSERT_EQ(1, reader->sequence());

  ASSERT_EQ(WriteResult::Repeat, writer->write(m2));
  ASSERT_EQ(2, writer->sequence());  // empty message is written

  ASSERT_EQ(WriteResult::Repeat, writer->write(m2));
  ASSERT_EQ(2, writer->sequence());  // reader hasn't moved yet

  ASSERT_TRUE(reader->next().empty());
  ASSERT_EQ(2, reader->sequence());  // empty message is read

  ASSERT_EQ(WriteResult::Success, writer->write(m2));
  ASSERT_EQ(3, writer->sequence());

  ASSERT_TRUE(expect_eq(m2, reader->next()));
  ASSERT_EQ(3, reader->sequence());

  array<uint8_t, M> m3{3};
  ASSERT_EQ(WriteResult::Repeat, writer->write(m3));
  ASSERT_EQ(4, writer->sequence());  // empty message written

  ASSERT_TRUE(reader->next().empty());
  ASSERT_EQ(4, reader->sequence());

  ASSERT_EQ(WriteResult::Success, writer->write(m3));
  ASSERT_EQ(5, writer->sequence());

  ASSERT_TRUE(expect_eq(m3, reader->next()));
  ASSERT_EQ(5, reader->sequence());

  ASSERT_TRUE(reader->next().empty());
  ASSERT_EQ(5, reader->sequence());
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

  ASSERT_EQ(WriteResult::Success, result);
  ASSERT_EQ(1, writer->sequence());

  const span<const uint8_t> read = reader->next();

  ASSERT_EQ(1, reader->sequence());
  ASSERT_TRUE(expect_eq(read, message.t));
}
}  // namespace

TEST(BlockingDemuxTest, WriteRead1) {
  rc::check(write_and_read_1<true>);
}

TEST(NonBlockingDemuxTest, WriteRead1) {
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

  LOG_DEBUG << "message number: " << messages.size();
  LOG_DEBUG << "message lengths: " << str;
  LOG_DEBUG << "messages: " << messages;

  const size_t message_num = messages.size();

  DemuxSetup<L, M, Blocking> setup(1);
  auto* writer = setup.writer();
  auto* reader = setup.reader(0);

  std::future<size_t> sent_count_future =
      std::async(std::launch::async, [&messages, &writer] { return write_all(messages, writer); });

  std::future<vector<TestMessage>> received_messages_future =
      std::async(std::launch::async, [message_num, &reader] { return read_n<L, M, Blocking>(message_num, reader); });

  sent_count_future.wait_for(DEFAULT_WAIT);
  ASSERT_TRUE(sent_count_future.valid());

  const size_t sent_count = sent_count_future.get();
  ASSERT_EQ(message_num, sent_count);

  received_messages_future.wait_for(DEFAULT_WAIT);
  ASSERT_TRUE(received_messages_future.valid());

  const auto received_messages = received_messages_future.get();
  assert_eq(messages, received_messages);
}
}  // namespace

TEST(BlockingDemuxTest, OneReaderReadX) {
  rc::check(one_reader_read_x<true>);
}

TEST(NonBlockingDemuxTest, OneReaderReadX) {
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
  ASSERT_TRUE(future_pub_result.valid());
  ASSERT_EQ(message_num, future_pub_result.get());

  for (auto& future_sub_result : future_sub_results) {
    future_sub_result.wait_for(DEFAULT_WAIT);
    ASSERT_TRUE(future_sub_result.valid());
    assert_eq(valid_messages, future_sub_result.get());
  }
}
}  // namespace

TEST(BlockingDemuxTest, MultipleReadersReadX) {
  rc::check(multiple_readers_read_x<true>);
}

TEST(NonBlockingDemuxTest, MultipleReadersReadX) {
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

auto main(int argc, char** argv) -> int {
  namespace logging = boost::log;
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::debug);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
*/
// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)