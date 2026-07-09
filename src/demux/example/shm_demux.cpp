// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#define XXH_INLINE_ALL  // <xxhash.h>

// Tell Boost to use the high-performance libbacktrace backend
// #define BOOST_STACKTRACE_USE_BACKTRACE
// Tell Boost to user add2line for extracting stack trace, which forks a new process and slow
// #define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>  // NOLINT(misc-include-cleaner)

#include <atomic>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>
#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>  // NOLINT(misc-include-cleaner)
#include <boost/log/trivial.hpp>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "../core/demux_reader.h"
#include "../core/demux_writer.h"
#include "../core/reader_id.h"
#include "../util/atomic_util.h"
#include "../util/boost_log_util.h"
#include "../util/hdr_histogram_util.h"
#include "../util/operators.h"
#include "../util/shm_manager.h"
#include "../util/string_util.h"
#include "../util/xxhash_util.h"
#include "./market_data.h"
#include "./shm_demux.h"

namespace {
auto print_usage(const char* prog) -> void {
  std::cerr << "Usage: " << prog << " [writer <number-of-readers> <number-of-messages> <zero-copy>]"
            << " | [reader <unique-reader-number> <number-of-messages> <zero-copy>]\n"
            << "  where\n"
            << "    <number-of-readers> and <unique-reader-number> are within the interval [1, "
            << static_cast<int>(std::numeric_limits<std::uint8_t>::max()) << "]\n"
            << "    <number-of-messages> is within the interval [1, " << std::numeric_limits<uint64_t>::max()
            << "] (uint64_t)\n"
            << "    <zero-copy> true/false\n";
}
}  // namespace

auto main(int argc, char* argv[]) noexcept -> int {
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);

  constexpr int ERROR = 100;
  try {
    const auto args = std::span<char*>(argv, static_cast<size_t>(argc));
    return lshl::demux::example::main_(args);
  } catch (...) {
    boost::stacktrace::stacktrace trace = boost::stacktrace::stacktrace::from_current_exception();
    LOG_ERROR << "exception: " << boost::current_exception_diagnostic_information(true) << ", trace: " << trace;
    return ERROR;
  }
}

namespace lshl::demux::example {

constexpr std::size_t MAX_READER_NUM = 6;

constexpr int REPORT_PROGRESS = 1000000;

constexpr std::size_t BUFFER_SIZE = 16 * lshl::demux::util::LINUX_PAGE_SIZE;

// max message size that would be allowed
constexpr std::uint16_t MAX_MESSAGE_SIZE = 256;

namespace bipc = boost::interprocess;

using lshl::demux::core::DemuxReader;
using lshl::demux::core::DemuxWriter;
using lshl::demux::core::ReaderId;
using lshl::demux::core::WriteResult;
using lshl::demux::util::HDR_histogram_util;
using lshl::demux::util::XXH64_util;
using std::atomic;
using std::size_t;
using std::span;
using std::uint16_t;
using std::vector;

auto main_(const span<char*> args) noexcept(false) -> int {
  constexpr int ERROR = 200;
  constexpr size_t EXPECTED_ARG_NUM = 5;

  if (args.size() != EXPECTED_ARG_NUM) {
    print_usage(args[0]);
    return ERROR;
  }

  const std::string command(args[1]);

  const auto msg_num = boost::lexical_cast<uint64_t>(args[3]);
  const auto zero_copy = std::string("true") == args[4];

  // TODO(Leonid): pass it as a command line argument
  const std::string shared_memory_name = "lshl_demux_buf";

  if (command == "writer") {
    const vector<uint16_t> ids = util::parse_vector<uint16_t>(std::string(args[2]));
    vector<ReaderId> reader_ids{};
    reader_ids.reserve(ids.size());
    for (const auto& x : ids) {
      reader_ids.emplace_back(static_cast<uint8_t>(x));
    }
    start_writer<BUFFER_SIZE, MAX_MESSAGE_SIZE, MAX_READER_NUM>(shared_memory_name, reader_ids, msg_num, zero_copy);
  } else if (command == "reader") {
    const auto id = static_cast<uint8_t>(boost::lexical_cast<uint16_t>(args[2]));
    start_reader<BUFFER_SIZE, MAX_MESSAGE_SIZE, MAX_READER_NUM>(shared_memory_name, ReaderId(id), msg_num);
  } else {
    print_usage(args[0]);
    return ERROR;
  }

  return 0;
}

template <size_t L, uint16_t M, size_t R>
auto start_writer(
    const string& shared_memory_name,
    const vector<ReaderId>& reader_ids,
    const uint64_t msg_num,
    bool zero_copy
) noexcept(false) -> void {
  constexpr size_t SHM_SIZE = 262144;  // lshl ::demux::util::calculate_shared_mem_size<L, R>();
  util::ShmManager<L, R> shm_manager{bipc::create_only, shared_memory_name, SHM_SIZE};

  util::ShmWriterData<L>* writer_data = shm_manager.construct_shm_writer_data();
  util::ShmReaderData<R>* reader_data = shm_manager.construct_shm_reader_data();

  span<uint8_t, L> buffer = writer_data->buffer.value;
  atomic<uint64_t>* downstream_sequence = &writer_data->downstream_sequence.value;
  const vector<const atomic<uint64_t>*>& all_upstream_sequences =
      util::to_const_pointer_vector(util::to_upstream_sequence_pointers(reader_data->upstream_sequences));

  vector<const atomic<uint64_t>*> filtered_upstream_sequences{};
  filtered_upstream_sequences.reserve(reader_ids.size());
  for (const auto& r : reader_ids) {
    filtered_upstream_sequences.push_back(all_upstream_sequences.at(r.value()));
  }

  DemuxWriter<L, M, false> writer(buffer, downstream_sequence, filtered_upstream_sequences);

  const atomic<size_t>* reader_count = &reader_data->active_reader_count.value;

  LOG_INFO << "waiting for all readers: " << util::log_vector(reader_ids) << "...";
  util::wait_for_count_ipc<size_t>(reader_count, reader_ids.size());
  LOG_INFO << "all readers connected";

  if (zero_copy) {
    run_writer_loop_zero_copy(&writer, msg_num);
  } else {
    run_writer_loop(&writer, msg_num);
  }
  LOG_INFO << "DemuxWriter completed";
}

template <size_t L, uint16_t M>
auto run_writer_loop(DemuxWriter<L, M, false>* writer, const uint64_t msg_num) noexcept(false) -> void {
  LOG_INFO << "sending " << msg_num << " md updates ...";

  MarketDataUpdate md{};
  MarketDataUpdateGenerator md_gen{};
  XXH64_util hash{};

  for (uint64_t i = 1; i <= msg_num; ++i) {
    md_gen.generate_market_data_update(&md);
    LOG_DEBUG << md;
    const bool ok = write(writer, md);
    if (!ok) {
      LOG_ERROR << "dropping message, could not write: " << md;
      continue;
    }
    if (i % REPORT_PROGRESS == 0) {
      LOG_INFO << "number of messages sent: " << i;
    }
    hash.update(&md, sizeof(MarketDataUpdate));
  }

  LOG_INFO << "writer sequence number: " << writer->message_count()
           << ", XXH64_hash: " << XXH64_util::format(hash.digest());
}

template <class T, size_t L, uint16_t M>
[[nodiscard]] inline auto write(DemuxWriter<L, M, false>* writer, const T& md) noexcept -> bool {
  static vector<uint64_t> upstream_sequences;
  int attempt = 0;

  while (true) {
    const WriteResult result = writer->write_safe(md);
    switch (result) {
      case WriteResult::Success:
        return true;
      case WriteResult::Error:
        return false;
      case WriteResult::Repeat:
        attempt += 1;
        if (attempt % REPORT_PROGRESS == 0) {
          writer->upstream_sequences(&upstream_sequences);
          LOG_WARNING << "one or more readers are lagging, wraparound is blocked, write attempt: " << attempt
                      << ", writer sequence: " << writer->message_count()
                      << ", downstream sequence: " << writer->downstream_sequence()
                      << ", upstream sequences: " << util::log_vector{upstream_sequences};
        }
        continue;
    }
  }
}

template <size_t L, uint16_t M>
auto run_writer_loop_zero_copy(DemuxWriter<L, M, false>* writer, const uint64_t msg_num) noexcept(false) -> void {
  LOG_INFO << "sending " << msg_num << " md updates ...";

  MarketDataUpdateGenerator md_gen{};
  XXH64_util hash{};

  for (uint64_t i = 1; i <= msg_num; ++i) {
    const bool ok = write_zero_copy(writer, &md_gen, &hash);
    if (!ok) {
      LOG_ERROR << "dropped one message, could not write";
      continue;
    }
    if (i % REPORT_PROGRESS == 0) {
      LOG_INFO << "number of messages sent: " << i;
    }
  }

  LOG_INFO << "writer sequence number: " << writer->message_count()
           << ", XXH64_hash: " << XXH64_util::format(hash.digest());
}

template <size_t L, uint16_t M>
[[nodiscard]] inline auto
write_zero_copy(DemuxWriter<L, M, false>* writer, MarketDataUpdateGenerator* md_gen, XXH64_util* hash) noexcept(false)
    -> bool {
  for (int attempt = 0;; ++attempt) {
    const std::optional<MarketDataUpdate*> mo = writer->template allocate<MarketDataUpdate>();
    if (mo.has_value()) {
      MarketDataUpdate* md = mo.value();
      LOG_DEBUG << md;
      md_gen->generate_market_data_update(md);
      writer->template commit<MarketDataUpdate>();
      hash->update(md, sizeof(MarketDataUpdate));
      return true;
    } else {
      attempt += 1;
      if (attempt % REPORT_PROGRESS == 0) {
        LOG_WARNING << "one or more readers are lagging, wraparound is blocked, write attempt: " << attempt
                    << ", writer sequence: " << writer->message_count();
      }
    }
  }
}

template <size_t L, uint16_t M, size_t R>
auto start_reader(const string& shared_memory_name, const ReaderId& reader_id, const uint64_t msg_num) noexcept(false)
    -> void {
  assert(reader_id.value() < MAX_READER_NUM);
  util::ShmManager<L, R> shm_manager{bipc::open_only, shared_memory_name};

  util::ShmWriterData<L>* writer_data = shm_manager.find_shm_writer_data();
  util::ShmReaderData<R>* reader_data = shm_manager.find_shm_reader_data();

  const span<uint8_t, L> buffer = writer_data->buffer.value;
  atomic<uint64_t>* downstream_sequence = &writer_data->downstream_sequence.value;
  vector<atomic<uint64_t>*> upstream_sequences = util::to_upstream_sequence_pointers(reader_data->upstream_sequences);

  DemuxReader<L, M> reader(reader_id, buffer, downstream_sequence, upstream_sequences[reader_id.value()]);

  atomic<size_t>* reader_count = &reader_data->active_reader_count.value;

  const auto active_reader_count = util::increment_count_ipc<size_t>(reader_count);
  LOG_INFO << "active_reader_count: " << active_reader_count;

  run_reader_loop(&reader, msg_num);
  LOG_INFO << "DemuxReader completed";
}

template <size_t L, uint16_t M>
auto run_reader_loop(DemuxReader<L, M>* reader, const uint64_t msg_num) noexcept(false) -> void {
  XXH64_util hash{};
  HDR_histogram_util histogram{};

  // consume the expected number of messages
  for (uint64_t i = 0; i < msg_num;) {
    const std::optional<const MarketDataUpdate*> read = reader->template next_unsafe<MarketDataUpdate>();
    if (read.has_value()) {
      i += 1;
      const MarketDataUpdate* md = read.value();
      // track the latency
      histogram.record_value(calculate_latency(md->timestamp));
      LOG_DEBUG << *md;
      // report progress
      if (i % REPORT_PROGRESS == 0) {
        LOG_INFO << "number of messages received: " << i;
      }
      // calculate the hash
      hash.update(md, sizeof(MarketDataUpdate));
    }
  }

  LOG_INFO << "reader sequence number: " << reader->message_count()
           << ", XXH64_hash: " << XXH64_util::format(hash.digest());

  LOG_INFO << "message latency, ns:";
  histogram.print_report();
}

auto inline calculate_latency(const uint64_t x0) -> int64_t {
  const std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> now =
      std::chrono::steady_clock::now();
  const uint64_t x1 = static_cast<uint64_t>(now.time_since_epoch().count());
  return static_cast<int64_t>(x1 - x0);
}

}  // namespace lshl::demux::example
