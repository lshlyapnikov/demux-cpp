// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner)

#include <rapidcheck/gen/Container.h>
#include <rapidcheck/gen/Select.h>
#define UNIT_TEST
#undef NDEBUG  // for assert to work in release build

#include <gtest/gtest.h>
#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include <array>
#include <atomic>
#include <boost/log/core.hpp>         // NOLINT(misc-include-cleaner)
#include <boost/log/expressions.hpp>  // NOLINT(misc-include-cleaner)
#include <boost/serialization/strong_typedef.hpp>
#include <boost/static_string/static_string.hpp>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <span>
#include <tuple>
#include <vector>
#include "../core/demultiplexer.h"
#include "../core/message_buffer.h"
#include "../core/reader_id.h"
#include "../util/tuple_util.h"

constexpr std::chrono::seconds DEFAULT_WAIT(5);

namespace {

enum class Side : std::uint8_t { Bid, Ask };

[[maybe_unused]] // it is actually used, but clangd thinks it isn't
auto operator<<(std::ostream& os, const Side& x) -> std::ostream& {
  switch (x) {
    case Side::Bid:
      os << "Bid";
      break;
    case Side::Ask:
      os << "Ask";
      break;
  }
  return os;
}

constexpr std::size_t MAX_SYMBOL_LEN = 8;
using Symbol = boost::static_string<MAX_SYMBOL_LEN>;

using MarketDataTick = std::tuple<
    Symbol,          // symbol
    Side,            // side
    std::uint16_t,   // price-level
    std::uint64_t,   // exchange timestamp
    std::uint64_t,   // price
    std::uint32_t>;  // size

}  // namespace

using lshl::demux::core::DemuxReader;
using lshl::demux::core::DemuxWriter;
using lshl::demux::core::ReaderId;
using std::array;
using std::atomic;
using std::optional;
using std::span;
using std::uint16_t;
using std::uint8_t;
using std::vector;

template <>
struct rc::Arbitrary<Side> {
  static auto arbitrary() -> Gen<Side> { return gen::element(Side::Ask, Side::Bid); }
};

template <>
struct rc::Arbitrary<Symbol> {
  static auto arbitrary() -> Gen<Symbol> {
    const Gen<uint8_t> nums = gen::inRange(static_cast<uint8_t>('0'), static_cast<uint8_t>('9'));
    const Gen<uint8_t> chars = gen::inRange(static_cast<uint8_t>('A'), static_cast<uint8_t>('Z'));
    const Gen<vector<uint8_t>> symbols = gen::container<vector<uint8_t>>(MAX_SYMBOL_LEN, gen::oneOf(nums, chars));
    return gen::map(symbols, [](const vector<uint8_t>& xs) { return Symbol{xs.begin(), xs.end()}; });
  }
};

constexpr uint16_t M = sizeof(MarketDataTick);
constexpr size_t L = 4 * lshl::demux::core::MessageBuffer<0>::required<MarketDataTick>();

namespace {
template <size_t L, uint16_t M, bool B>
auto write_all_with_allocate(const vector<MarketDataTick>& messages, DemuxWriter<L, M, B>& writer) -> size_t {
  using lshl::demux::util::operator<<;

  size_t result = 0;
  for (size_t i = 0; i < messages.size();) {
    const optional<MarketDataTick*> m1_opt = writer.template allocate<MarketDataTick>();
    if (m1_opt.has_value()) {
      MarketDataTick* m1 = m1_opt.value();
      *m1 = messages[i];
      writer.template commit<MarketDataTick>();
      std::cout << "written: " << *m1 << "\n";
      i += 1;
      result += 1;
    } else {
      // keep trying until the allocation succeeded or the test is timed out
    }
  }

  return result;
}

template <size_t L, uint16_t M>
auto read_n(const size_t message_num, DemuxReader<L, M>& reader) -> vector<MarketDataTick> {
  using lshl::demux::util::operator<<;

  vector<MarketDataTick> result;
  result.reserve(message_num);

  while (result.size() < message_num) {
    const optional<const MarketDataTick*> mo = reader.template next_unsafe<MarketDataTick>();
    if (mo.has_value()) {
      result.push_back(*mo.value());
    }
  }

  // read one more to unblock the reader, which might be waiting for the wraparound unblock
  const optional<const MarketDataTick*> m = reader.template next_unsafe<MarketDataTick>();
  assert(!m.has_value());

  return result;
}

template <bool Blocking>
auto multiple_readers_read_x(const vector<MarketDataTick>& messages) -> bool {
  using lshl::demux::util::operator<<;

  if (messages.empty()) {
    return true;
  }

  std::cout << '[';
  for (const auto& x : messages) {
    std::cout << x << ", ";
  }
  std::cout << "]\n";

  constexpr uint8_t READER_NUM = 7;

  const size_t message_num = messages.size();

  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const uint64_t all_readers_mask = ReaderId::all_readers_mask(READER_NUM);

  vector<DemuxReader<L, M>> readers{};
  readers.reserve(READER_NUM);
  for (uint8_t i = 1; i <= READER_NUM; ++i) {
    const ReaderId id(i);
    readers.emplace_back(id, span{buffer}, &msg_counter_sync, &wraparound_sync);
  }

  DemuxWriter<L, M, Blocking> writer(all_readers_mask, span{buffer}, &msg_counter_sync, &wraparound_sync);

  std::future<size_t> future_writer_result =
      std::async(std::launch::async, [&messages, &writer] { return write_all_with_allocate(messages, writer); });

  vector<std::future<vector<MarketDataTick>>> future_reader_results{};
  future_reader_results.reserve(READER_NUM);
  for (auto& reader : readers) {
    future_reader_results.emplace_back(std::async(std::launch::async, [message_num, &reader] {
      return read_n(message_num, reader);
    }));
  }

  future_writer_result.wait_for(DEFAULT_WAIT);
  EXPECT_TRUE(future_writer_result.valid());
  EXPECT_EQ(message_num, future_writer_result.get());

  for (auto& future_reader_result : future_reader_results) {
    future_reader_result.wait_for(DEFAULT_WAIT);
    EXPECT_TRUE(future_reader_result.valid());
    EXPECT_EQ(messages, future_reader_result.get());
    if (::testing::Test::HasFailure()) {
      return false;
    }
  }

  return !::testing::Test::HasFailure();
}

}  // namespace

TEST(BlockingDemuxWriterWithAllocateTest, AllocateAndCommitWithMultipleReadersReadX) {
  rc::check(multiple_readers_read_x<true>);
}

TEST(NonBlockingDemuxWriterWithAllocateTest, AllocateAndCommitWithMultipleReadersReadX) {
  rc::check(multiple_readers_read_x<false>);
}

TEST(SymbolGenerator, SymbolDistribution) {
  rc::check([](const Symbol& symbol) { RC_TAG(symbol); });
}

namespace {
auto fill_up_buffer(DemuxWriter<L, M, false>* writer, const MarketDataTick& message) -> void {
  while (true) {
    const optional<MarketDataTick*> m_opt = writer->template allocate<MarketDataTick>();
    if (m_opt.has_value()) {
      MarketDataTick* m = m_opt.value();
      *m = message;
      writer->template commit<MarketDataTick>();
    } else {
      return;
    }
  }
}

auto read_all_expect_eq(DemuxReader<L, M>* reader, const MarketDataTick& expected) -> bool {
  while (true) {
    const std::optional<const MarketDataTick*> read = reader->next_unsafe<MarketDataTick>();
    if (read.has_value()) {
      EXPECT_EQ(expected, *read.value());
      if (::testing::Test::HasFailure()) {
        return false;
      }
    } else {
      return true;
    }
  }
}

auto slow_reader_test(const MarketDataTick& message) -> bool {
  array<uint8_t, L> buffer{};
  atomic<uint64_t> msg_counter_sync{0};
  atomic<uint64_t> wraparound_sync{0};
  const ReaderId reader_id{1};

  DemuxWriter<L, M, false> writer(0, span{buffer}, &msg_counter_sync, &wraparound_sync);
  DemuxReader<L, M> reader(reader_id, span{buffer}, &msg_counter_sync, &wraparound_sync);
  writer.add_reader(reader_id);

  EXPECT_EQ(vector{reader_id}, writer.lagging_readers());

  fill_up_buffer(&writer, message);

  // the buffer is full, can't write into it
  EXPECT_FALSE(writer.allocate<MarketDataTick>().has_value());
  EXPECT_EQ(vector{reader_id}, writer.lagging_readers());

  read_all_expect_eq(&reader, message);
  EXPECT_TRUE(writer.lagging_readers().empty());

  // all readers caught up, can write again
  const auto x = writer.template allocate<MarketDataTick>();
  writer.template commit<MarketDataTick>();
  EXPECT_TRUE(x.has_value());

  return !::testing::Test::HasFailure();
}
}  // namespace

TEST(NonBlockingDemuxWriterWithAllocateTest, SlowReader) {
  rc::check(slow_reader_test);
}

auto main(int argc, char** argv) -> int {
  namespace logging = boost::log;
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::warning);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)