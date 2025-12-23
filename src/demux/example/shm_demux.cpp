// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#define XXH_INLINE_ALL  // <xxhash.h>

#include "./shm_demux.h"
#include <atomic>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/interprocess/creation_tags.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/expressions.hpp>  // NOLINT(misc-include-cleaner)
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>  // NOLINT(misc-include-cleaner)
#include <cassert>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
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
#include "../util/xxhash_util.h"
#include "./market_event.h"

namespace {
auto print_usage(const char* prog) -> void {
  using lshl::demux::core::MAX_READER_NUM;
  std::cerr << "Usage: " << prog << " [writer <number-of-readers> <number-of-messages> <zero-copy>]"
            << " | [reader <unique-reader-number> <number-of-messages> <zero-copy>]\n"
            << "  where\n"
            << "    <number-of-readers> and <unique-reader-number> are within the interval [1, "
            << static_cast<int>(MAX_READER_NUM) << "]\n"
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
  using lshl::demux::core::MAX_READER_NUM;

  constexpr int ERROR = 200;
  constexpr size_t EXPECTED_ARG_NUM = 5;
  constexpr size_t BUFFER_SIZE = 32;

  init_logging();

  if (args.size() != EXPECTED_ARG_NUM) {
    print_usage(args[0]);
    return ERROR;
  }

  const std::string command(args[1]);
  const auto num16 = boost::lexical_cast<uint16_t>(args[2]);
  if (num16 > MAX_READER_NUM) {
    print_usage(args[0]);
    return ERROR;
  }
  const auto reader_num = static_cast<uint8_t>(num16);  // reader_id in case of the reader command
  const auto msg_num = boost::lexical_cast<uint64_t>(args[3]);
  const auto emplace = std::string("true") == args[4];

  if (reader_num > MAX_READER_NUM) {
    LOG_ERROR << "the requested number of readers: " << static_cast<int>(reader_num)
              << " must be less than or equal to the MAX_READER_NUM the application was compiled with: "
              << MAX_READER_NUM;
    return ERROR;
  }

  if (command == "writer") {
    start_writer<MarketDataUpdate, BUFFER_SIZE>(reader_num, msg_num, emplace);
  } else if (command == "reader") {
    start_reader<MarketDataUpdate, BUFFER_SIZE>(ReaderId{reader_num}, msg_num);
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
auto start_writer(const uint8_t total_reader_num, const uint64_t msg_num, bool emplace) noexcept(false) -> void {
  constexpr size_t SHARED_MEMORY_SIZE = 64 * util::LINUX_PAGE_SIZE;
  util::ShmManager<M, N> shm_manager{bipc::create_only, BUFFER_SHARED_MEM_NAME, SHARED_MEMORY_SIZE, total_reader_num};
  const std::atomic<size_t>* startup_reader_counter = shm_manager.construct_startup_reader_counter();

  DemuxWriter<M, N, false> writer(
      shm_manager.construct_buffer(),
      shm_manager.construct_writer_tail(),
      shm_manager.construct_all_reader_heads(),
      std::vector(total_reader_num, true)
  );

  wait_for_readers(startup_reader_counter, total_reader_num);

  if (emplace) {
    run_writer_loop(&writer, msg_num, write_with_emplace<M, N>);
  } else {
    run_writer_loop(&writer, msg_num, write<M, N>);
  }
  LOG_INFO << "DemuxWriter completed, shm_manager.free_memory: " << shm_manager.get_free_memory()
           << ", writer.msg_count: " << writer.message_count();
}

template <typename M, size_t N, typename WriteFn>
auto run_writer_loop(DemuxWriter<M, N, false>* writer, const uint64_t msg_num, WriteFn write_fn) noexcept(false)
    -> void {
  LOG_INFO << "sending " << msg_num << " md updates ...";

  M md{};
  MarketDataUpdateGenerator md_gen{};
  XXH64_util hash{};

  for (uint64_t i = 1; i <= msg_num; ++i) {
    md_gen.generate_market_data_update(&md);
    LOG_DEBUG << md;
    const bool ok = write_fn(writer, md);
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

template <typename M, size_t N>
[[nodiscard]] inline auto write(DemuxWriter<M, N, false>* writer, const M& md) noexcept -> bool {
  int attempt = 0;

  while (true) {
    std::optional<M*> ptr = writer->next();
    if (ptr.has_value()) {
      *(ptr.value()) = md;
      return writer->commit();
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
    }
    attempt += 1;
    if (attempt % REPORT_PROGRESS == 0) {
      LOG_WARNING << "one or more readers are lagging, wraparound is blocked, write attempt: " << attempt
                  << ", writer tail: " << writer->tail();
    }
  }
}

template <typename M, size_t N>
auto start_reader(const ReaderId& reader_id, const uint64_t msg_num) noexcept(false) -> void {
  using lshl::demux::example::BUFFER_SHARED_MEM_NAME;
  using std::atomic;

  util::ShmManager<M, N> shm_manager{bipc::open_only, BUFFER_SHARED_MEM_NAME};
  std::atomic<size_t>* startup_reader_counter = shm_manager.find_startup_reader_counter();

  DemuxReader<M, N, false> reader(
      reader_id,
      shm_manager.find_buffer(),
      shm_manager.find_writer_tail(),
      shm_manager.find_reader_head(reader_id.value())
  );

  startup_reader_counter->fetch_add(1);

  run_reader_loop(&reader, msg_num);

  LOG_INFO << "DemuxReader completed, shm_manager.free_memory: " << shm_manager.get_free_memory()
           << ", reader.msg_count: " << reader.message_count();
}

template <typename M, size_t N>
auto run_reader_loop(DemuxReader<M, N, false>* reader, const uint64_t msg_num) noexcept(false) -> void {
  XXH64_util hash{};
  HDR_histogram_util histogram{};

  // consume the expected number of messages
  for (uint64_t i = 0; i < msg_num;) {
    const std::optional<const M*> read = reader->next();
    if (read.has_value()) {
      i += 1;
      const M* md = read.value();
      // track the latency
      histogram.record_value(calculate_latency(md->timestamp));
      LOG_DEBUG << *md;
      // report progress
      if (i % REPORT_PROGRESS == 0) {
        LOG_INFO << "number of messages received: " << i;
      }
      // calculate the hash
      hash.update(md, sizeof(M));
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
