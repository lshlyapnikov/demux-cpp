// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#include <ctime>
#define XXH_INLINE_ALL  // <xxhash.h>

#include <sched.h>
#include <atomic>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/interprocess/creation_tags.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/expressions.hpp>  // NOLINT(misc-include-cleaner)
#include "./shm_demux.h"
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>  // NOLINT(misc-include-cleaner)
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <thread>
#include <vector>
#include "../core/demux_reader.h"
#include "../core/demux_writer.h"
#include "../core/reader_id.h"
#include "../util/boost_log_util.h"
#include "../util/hdr_histogram_util.h"
#include "../util/shm_manager.h"
#include "../util/timestamp_util.h"
#include "../util/xxhash_util.h"
#include "./market_event.h"

namespace {
auto print_usage(const char* prog) -> void {
  using lshl::demux::example::READER_NUM;
  std::cerr << "Usage: " << prog << " [writer <number-of-readers> <number-of-messages> <zero-copy>]"
            << " | [reader <unique-reader-number> <number-of-messages> <zero-copy>]\n"
            << "  where\n"
            << "    <number-of-readers> and <unique-reader-number> are within the interval [0, " << READER_NUM << ")\n"
            << "    <number-of-messages> is within the interval [1, " << std::numeric_limits<uint64_t>::max()
            << "] (uint64_t)\n"
            << "    <use-emplace> true/false\n";
}

auto shutdown_handler() -> void {
  LOG_INFO << "[shutdown_handler] trace: " << boost::stacktrace::stacktrace() << "\n" << "Shutting down... ";
  std::abort();
}

}  // namespace

auto main(int argc, char* argv[]) noexcept -> int {
  std::set_terminate(&shutdown_handler);
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

constexpr std::string BUFFER_SHARED_MEM_NAME{"lshl_demux_buf"};

constexpr int REPORT_PROGRESS = 1000000;

namespace bipc = boost::interprocess;

using lshl::demux::core::DemuxReader;
using lshl::demux::core::DemuxWriter;
using lshl::demux::core::ReaderId;
using lshl::demux::util::HDR_histogram_util;
using lshl::demux::util::XXH64_util;
using std::size_t;
using std::span;
using std::uint16_t;

auto main_(const span<char*> args) noexcept(false) -> int {
  constexpr int ERROR = 200;
  constexpr size_t EXPECTED_ARG_NUM = 6;

  init_logging();

  if (args.size() != EXPECTED_ARG_NUM) {
    print_usage(args[0]);
    return ERROR;
  }

  const std::string command(args[1]);
  const auto num16 = boost::lexical_cast<uint16_t>(args[2]);
  if (num16 > READER_NUM) {
    print_usage(args[0]);
    return ERROR;
  }
  const auto reader_num = static_cast<uint8_t>(num16);  // reader_id in case of the reader command
  const auto msg_num = boost::lexical_cast<uint64_t>(args[3]);
  const auto emplace = std::string("true") == args[4];
  const auto calculate_hash = std::string("true") == args[5];

  if (reader_num > READER_NUM) {
    LOG_ERROR << "the requested number of readers: " << static_cast<int>(reader_num)
              << " must be less than or equal to the READER_NUM the application was compiled with: " << READER_NUM;
    return ERROR;
  }

  int cpu_id = sched_getcpu();
  LOG_INFO << "cpu: " << cpu_id << ", command: " << command << ", reader_num: " << static_cast<int>(reader_num)
           << ", msg_num: " << msg_num << ", emplace: " << emplace;

  if (command == "writer") {
    start_writer<MarketDataUpdate, BUFFER_SIZE>(reader_num, msg_num, emplace, calculate_hash);
  } else if (command == "reader") {
    start_reader<MarketDataUpdate, BUFFER_SIZE, BLOCKING>(ReaderId{reader_num}, msg_num, calculate_hash);
  } else {
    print_usage(args[0]);
    return ERROR;
  }

  return 0;
}

auto init_logging() noexcept -> void {
  // NOLINTNEXTLINE(misc-include-cleaner)
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
}

auto wait_for_readers(const std::atomic<size_t>* startup_reader_counter, const uint8_t total_reader_num) -> void {
  LOG_INFO << "waiting for all readers to start...";
  while (true) {
    const size_t x = startup_reader_counter->load();
    if (x == total_reader_num) {
      break;
    } else {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(1s);  // NOLINT(misc-include-cleaner)
    }
  }
  LOG_INFO << "all readers started";
}

template <typename M, size_t N>
auto start_writer(
    const uint8_t total_reader_num,
    const uint64_t msg_num,
    const bool emplace,
    const bool calculate_hash
) noexcept(false) -> void {
  if (total_reader_num != 2) {
    throw std::runtime_error("Epxected exactly 2 readers, TODO: modify it to support any number of readers");
  }
  constexpr size_t SHARED_MEMORY_SIZE = 64 * util::LINUX_PAGE_SIZE;
  util::ShmManager<M, N, READER_NUM> shm_manager{
      bipc::create_only, BUFFER_SHARED_MEM_NAME, SHARED_MEMORY_SIZE, total_reader_num
  };
  const std::atomic<size_t>* startup_reader_counter = shm_manager.construct_startup_reader_counter();

  util::ShmData<M, N, READER_NUM>* shm_data = shm_manager.construct_shm_data();

  std::vector<const std::atomic<size_t>*> heads{};
  heads.reserve(READER_NUM);
  for (auto& x : shm_data->heads) {
    heads.push_back(&x.value);
  }

  DemuxWriter<M, N, false> writer(&shm_data->buffer, &shm_data->tail, heads, std::vector(total_reader_num, true));

  // pre-load the buffer into L1 cache
  for (M m : shm_data->buffer) {
    m.timestamp = 0;
  }

  wait_for_readers(startup_reader_counter, total_reader_num);

  if (emplace) {
    run_writer_loop(&writer, msg_num, calculate_hash, write_with_emplace<M, N>);
  } else {
    run_writer_loop(&writer, msg_num, calculate_hash, write<M, N>);
  }
  LOG_INFO << "DemuxWriter completed, shm_manager.free_memory: " << shm_manager.get_free_memory()
           << ", writer.msg_count: " << writer.message_count();
}

template <typename M, size_t N, typename WriteFn>
auto run_writer_loop(
    DemuxWriter<M, N, false>* writer,
    const uint64_t msg_num,
    const bool calculate_hash,
    WriteFn write_fn
) noexcept(false) -> void {
  LOG_INFO << "sending " << msg_num << " md updates, calculate_hash: " << calculate_hash << " ...";
  M md{};
  MarketDataUpdateGenerator md_gen{};
  XXH64_util hash{};

  const uint64_t ns0 = util::timestamp_ns(CLOCK_MONOTONIC);

  for (uint64_t i = 1; i <= msg_num; ++i) {
    md_gen.generate_market_data_update(&md);
    LOG_DEBUG << md;
    const bool ok = write_fn(writer, md);
    if (!ok) {
      LOG_ERROR << "dropping message, could not write: " << md;
      continue;
    }
    if (calculate_hash) {
      hash.update(&md, sizeof(MarketDataUpdate));
    }
  }

  const uint64_t ns1 = util::timestamp_ns(CLOCK_MONOTONIC);

  LOG_INFO << "writer sequence number: " << writer->message_count()
           << ", XXH64_hash: " << XXH64_util::format(hash.digest());

  const uint64_t delta_ns = ns1 - ns0;
  const double average_latency = static_cast<double>(delta_ns) / static_cast<double>(writer->message_count());

  LOG_INFO << "writer sent: " << writer->message_count() << " events in " << delta_ns << " ns"
           << ", average latency: " << average_latency << " ns";
}

template <typename M, size_t N>
[[nodiscard]] inline auto write(DemuxWriter<M, N, false>* writer, const M& md) noexcept -> bool {
  int attempt = 0;

  while (true) {
    M* ptr = writer->next();
    if (ptr) {
      *ptr = md;
      return writer->commit();
    } else {
      _mm_pause();
    }
    attempt += 1;
    if (attempt % REPORT_PROGRESS == 0) {
      LOG_WARNING << "one or more readers are lagging, wraparound is blocked, write attempt: " << attempt
                  << ", writer tail: " << writer->tail();
    }
  }
}

template <typename M, size_t N>
[[nodiscard]] inline auto write_with_emplace(DemuxWriter<M, N, false>* writer, const M& md) noexcept -> bool {
  int attempt = 0;

  while (true) {
    if (writer->emplace(md)) {
      return writer->commit();
    } else {
      _mm_pause();
    }
    attempt += 1;
    if (attempt % REPORT_PROGRESS == 0) {
      LOG_WARNING << "one or more readers are lagging, wraparound is blocked, write attempt: " << attempt
                  << ", writer tail: " << writer->tail();
    }
  }
}

template <typename M, size_t N, bool B>
auto start_reader(const ReaderId& reader_id, const uint64_t msg_num, const bool calculate_hash) noexcept(false)
    -> void {
  using lshl::demux::example::BUFFER_SHARED_MEM_NAME;
  using std::atomic;

  util::ShmManager<M, N, READER_NUM> shm_manager{bipc::open_only, BUFFER_SHARED_MEM_NAME};
  std::atomic<size_t>* startup_reader_counter = shm_manager.find_startup_reader_counter();

  util::ShmData<M, N, READER_NUM>* shm_data = shm_manager.find_shm_data();

  DemuxReader<M, N, B> reader(
      reader_id, &shm_data->buffer, &shm_data->tail, &shm_data->heads.at(reader_id.value()).value
  );

  // pre-load the buffer into L1 cache
  for (M m : shm_data->buffer) {
    if (m.timestamp != 0) {
      LOG_WARNING << "pre-loaded m" << m;
    }
  }

  startup_reader_counter->fetch_add(1);

  run_reader_loop(&reader, msg_num, calculate_hash);

  LOG_INFO << "DemuxReader completed, shm_manager.free_memory: " << shm_manager.get_free_memory()
           << ", reader.msg_count: " << reader.message_count();
}

/*
template <typename M, size_t N, bool B>
auto run_reader_loop(DemuxReader<M, N, B>* reader, const uint64_t msg_num) noexcept(false) -> void {
  XXH64_util hash{};
  HDR_histogram_util histogram{};

  // consume the expected number of messages
  for (uint64_t i = 0; i < msg_num;) {
    const M* const md = reader->next();
    if (md) {
      i += 1;
      // track the latency
      histogram.record_value(calculate_latency(md->timestamp));
      LOG_DEBUG << *md;
      // calculate the hash
      hash.update(md, sizeof(M));
    }
  }

  LOG_INFO << "reader sequence number: " << reader->message_count()
           << ", XXH64_hash: " << XXH64_util::format(hash.digest());

  LOG_INFO << "message latency, ns:";
  histogram.print_report();
}
*/

template <typename M, size_t N, bool B>
auto run_reader_loop(DemuxReader<M, N, B>* reader, const uint64_t msg_num, const bool calculate_hash) noexcept(false)
    -> void {
  XXH64_util hash{};
  HDR_histogram_util histogram{};
  M dummy_md{};

  LOG_INFO << "calculate_hash: " << calculate_hash;

  const util::TscConverter conv = util::calibrate_best_effort();

  const uint64_t ns0 = util::timestamp_ns(CLOCK_MONOTONIC);

  // consume the expected number of messages
  for (uint64_t i = 0; i < msg_num;) {
    // branchless optimization
    const M* const tmp_ptr = reader->next();
    const bool has_data = (tmp_ptr != nullptr);
    const M* const safe_ptr = has_data ? tmp_ptr : &dummy_md;
    LOG_DEBUG << *safe_ptr;

    // track the latency
    const uint64_t now = util::timestamp_counter();
    histogram.record_value(conv.ticks_to_ns(now - safe_ptr->timestamp) * static_cast<uint64_t>(has_data));

    // calculate the hash
    if (calculate_hash) {
      hash.update(safe_ptr, sizeof(M) * static_cast<size_t>(has_data));  // sizeof(M) * (1 or 0)
    }

    i += static_cast<uint64_t>(has_data);  // i += 1
  }

  const uint64_t ns1 = util::timestamp_ns(CLOCK_MONOTONIC);

  LOG_INFO << "reader sequence number: " << reader->message_count()
           << ", XXH64_hash: " << XXH64_util::format(hash.digest());

  const uint64_t delta_ns = ns1 - ns0;
  const double average_latency = static_cast<double>(delta_ns) / static_cast<double>(reader->message_count());

  LOG_INFO << "reader received: " << reader->message_count() << " events in " << delta_ns << " ns"
           << ", average latency: " << average_latency << " ns";

  LOG_INFO << "message latency, ns:";
  histogram.print_report();
}

}  // namespace lshl::demux::example
