// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(misc-include-cleaner, cppcoreguidelines-pro-bounds-array-to-pointer-decay)

#define XXH_INLINE_ALL  // <xxhash.h>

#include "./shm_demux.h"
#include <array>
#include <atomic>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include "../core/demultiplexer.h"
#include "../core/reader_id.h"
#include "../util/hdr_histogram_util.h"
#include "../util/shm_remover.h"
#include "../util/shm_util.h"
#include "../util/xxhash_util.h"
#include "./market_data.h"

auto print_usage(const char* prog) -> void {
  std::cerr << "Usage: " << prog << " [writer <number-of-readers> <number-of-messages>] "
            << " | [reader <unique-reader-number> <number-of-messages>]\n"
            << "  where\n"
            << "    <number-of-readers> and <unique-reader-number> are within the interval [1, "
            << static_cast<int>(lshl::demux::core::MAX_READER_NUM) << "]\n"
            << "    <number-of-messages> is within the interval [1, " << std::numeric_limits<uint64_t>::max()
            << "] (uint64_t)\n";
}

auto main(int argc, char* argv[]) noexcept -> int {
  constexpr int ERROR = 100;
  try {
    const auto args = std::span<char*>(argv, static_cast<size_t>(argc));
    return lshl::demux::example::main_(args);
  } catch (const boost::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "boost::exception: " << boost::diagnostic_information(e);
    return ERROR;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "std::exception: " << e.what();
    return ERROR;
  } catch (...) {
    BOOST_LOG_TRIVIAL(error) << "unexpected exception";
    return ERROR;
  }
}

namespace lshl::demux::example {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
constexpr char BUFFER_SHARED_MEM_NAME[] = "lshl_demux_buf";
// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
constexpr char UTIL_SHARED_MEM_NAME[] = "lshl_demux_util";

constexpr int REPORT_PROGRESS = 1000000;

// circular buffer size in bytes
constexpr std::size_t BUFFER_SIZE =
    16 * lshl::demux::util::LINUX_PAGE_SIZE - lshl::demux::util::BOOST_IPC_INTERNAL_METADATA_SIZE;

// max message size that would be allowed
constexpr std::uint16_t MAX_MESSAGE_SIZE = 256;

namespace bipc = boost::interprocess;

using lshl::demux::core::DemuxReader;
using lshl::demux::core::DemuxWriter;
using lshl::demux::core::ReaderId;
using lshl::demux::core::WriteResult;
using lshl::demux::util::HDR_histogram_util;
using lshl::demux::util::ShmRemover;
using lshl::demux::util::XXH64_util;
using std::array;
using std::atomic;
using std::size_t;
using std::span;
using std::uint16_t;

auto main_(const span<char*> args) noexcept(false) -> int {
  constexpr int ERROR = 200;

  init_logging();

  if (args.size() != 4) {
    print_usage(args[0]);
    return ERROR;
  }

  const std::string command(args[1]);
  const auto num16 = boost::lexical_cast<uint16_t>(args[2]);
  if (num16 < 1 || num16 > lshl::demux::core::MAX_READER_NUM) {
    print_usage(args[0]);
    return ERROR;
  }
  const auto num8 = static_cast<uint8_t>(num16);
  const auto msg_num = boost::lexical_cast<uint64_t>(args[3]);

  if (command == "writer") {
    start_writer<BUFFER_SIZE, MAX_MESSAGE_SIZE>(num8, msg_num);
  } else if (command == "reader") {
    start_reader<BUFFER_SIZE, MAX_MESSAGE_SIZE>(num8, msg_num);
  } else {
    print_usage(args[0]);
    return ERROR;
  }

  return 0;
}

auto init_logging() noexcept -> void {
  boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::info);
}

template <size_t L, uint16_t M>
auto start_writer(const uint8_t total_reader_num, const uint64_t msg_num) noexcept(false) -> void {
  const size_t SHM_SIZE = lshl::demux::util::calculate_required_shared_mem_size(
      L, lshl::demux::util::BOOST_IPC_INTERNAL_METADATA_SIZE, lshl::demux::util::LINUX_PAGE_SIZE
  );

  BOOST_LOG_TRIVIAL(info) << "start_writer " << BUFFER_SHARED_MEM_NAME << ", size: " << SHM_SIZE << ", L: " << L
                          << ", M: " << M << ", total_reader_num: " << static_cast<int>(total_reader_num);

  const ShmRemover remover1(BUFFER_SHARED_MEM_NAME);
  const ShmRemover remover2(UTIL_SHARED_MEM_NAME);

  const uint64_t all_subs_mask = ReaderId::all_readers_mask(total_reader_num);

  // segment for the circular buffer and message counter, written by writer, read by readers
  bipc::managed_shared_memory segment1(bipc::create_only, BUFFER_SHARED_MEM_NAME, SHM_SIZE);
  BOOST_LOG_TRIVIAL(info) << "created shared_memory_object segment1: " << BUFFER_SHARED_MEM_NAME
                          << ", segment1.free_memory: " << segment1.get_free_memory();

  array<uint8_t, L>* buffer = segment1.construct<array<uint8_t, L>>("buffer")();
  BOOST_LOG_TRIVIAL(info) << "buffer allocated, segment1.free_memory: " << segment1.get_free_memory();

  atomic<uint64_t>* message_count_sync = segment1.construct<atomic<uint64_t>>("message_count_sync")(0);
  BOOST_LOG_TRIVIAL(info) << "message_count_sync allocated, segment1.free_memory: " << segment1.get_free_memory();

  // segment for synchronization
  bipc::managed_shared_memory segment2(bipc::create_only, UTIL_SHARED_MEM_NAME, lshl::demux::util::LINUX_PAGE_SIZE);
  BOOST_LOG_TRIVIAL(info) << "created shared_memory_object segment2: " << UTIL_SHARED_MEM_NAME
                          << ", segment2.free_memory: " << segment2.get_free_memory();

  atomic<uint64_t>* wraparound_sync = segment2.construct<atomic<uint64_t>>("wraparound_sync")(0);
  BOOST_LOG_TRIVIAL(info) << "wraparound_sync allocated, segment2.free_memory: " << segment2.get_free_memory();

  atomic<uint64_t>* startup_sync = segment2.construct<atomic<uint64_t>>("startup_sync")(0);
  BOOST_LOG_TRIVIAL(info) << "startup_sync allocated, segment2.free_memory: " << segment2.get_free_memory();

  lshl::demux::core::DemuxWriter<L, M, false> pub(all_subs_mask, span{*buffer}, message_count_sync, wraparound_sync);
  BOOST_LOG_TRIVIAL(info) << "DemuxWriter created, segment1.free_memory: " << segment1.get_free_memory()
                          << ", segment2.free_memory: " << segment2.get_free_memory();

  BOOST_LOG_TRIVIAL(info) << "waiting for all readers ...";
  while (true) {
    const uint64_t x = startup_sync->load();
    if (x == all_subs_mask) {
      break;
    } else {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(1s);
    }
  }
  BOOST_LOG_TRIVIAL(info) << "all readers connected";

  run_writer_loop(pub, msg_num);
  BOOST_LOG_TRIVIAL(info) << "DemuxWriter completed, segment1.free_memory: " << segment1.get_free_memory()
                          << ", segment2.free_memory: " << segment2.get_free_memory();
}

template <size_t L, uint16_t M>
auto run_writer_loop(lshl::demux::core::DemuxWriter<L, M, false>& pub, const uint64_t msg_num) noexcept(false) -> void {
  BOOST_LOG_TRIVIAL(info) << "sending " << msg_num << " md updates ...";

  MarketDataUpdate md{};
  MarketDataUpdateGenerator md_gen{};
  XXH64_util hash{};

  for (uint64_t i = 1; i <= msg_num; ++i) {
    md_gen.generate_market_data_update(&md);
#ifndef NDEBUG
    BOOST_LOG_TRIVIAL(debug) << md;
#endif
    const bool ok = send_(pub, md);
    if (!ok) {
      BOOST_LOG_TRIVIAL(error) << "dropping message, could not write: " << md;
      continue;
    }
    if (i % REPORT_PROGRESS == 0) {
      BOOST_LOG_TRIVIAL(info) << "number of messages sent: " << i;
    }
    hash.update(&md, sizeof(MarketDataUpdate));
  }

  BOOST_LOG_TRIVIAL(info) << "writer sequence number: " << pub.message_count()
                          << ", XXH64_hash: " << XXH64_util::format(hash.digest());
}

template <class T, size_t L, uint16_t M>
[[nodiscard]] inline auto send_(DemuxWriter<L, M, false>& pub, const T& md) noexcept -> bool {
  int attempt = 0;
  while (true) {
    const WriteResult result = pub.write_object(md);
    switch (result) {
      case WriteResult::Success:
        return true;
      case WriteResult::Error:
        return false;
      case WriteResult::Repeat:
        attempt += 1;
        if (attempt % REPORT_PROGRESS == 0) {
          BOOST_LOG_TRIVIAL(warning) << "one or more readers are lagging, wraparound is blocked, write attempt: "
                                     << attempt << ", writer sequence: " << pub.message_count();
        }
        continue;
    }
  }
}

template <size_t L, uint16_t M>
auto start_reader(const uint8_t reader_num, const uint64_t msg_num) noexcept(false) -> void {
  using lshl::demux::example::BUFFER_SHARED_MEM_NAME;
  using std::atomic;

  BOOST_LOG_TRIVIAL(info) << "reader BUFFER_SHARED_MEM_NAME: " << BUFFER_SHARED_MEM_NAME << ", L: " << L << ", M: " << M
                          << ", reader_num: " << static_cast<int>(reader_num);

  // read-only segment for the circular buffer and message counter
  bipc::managed_shared_memory segment1(bipc::open_read_only, BUFFER_SHARED_MEM_NAME);
  BOOST_LOG_TRIVIAL(info) << "opened shared_memory_object segment1: " << BUFFER_SHARED_MEM_NAME
                          << ", segment1.free_memory: " << segment1.get_free_memory();

  array<uint8_t, L>* buffer = segment1.find<array<uint8_t, L>>("buffer").first;
  BOOST_LOG_TRIVIAL(info) << "buffer found, segment1.free_memory: " << segment1.get_free_memory();

  atomic<uint64_t>* message_count_sync = segment1.find<atomic<uint64_t>>("message_count_sync").first;
  BOOST_LOG_TRIVIAL(info) << "message_count_sync found, segment1.free_memory: " << segment1.get_free_memory();

  // read-write segment for atomic variables
  bipc::managed_shared_memory segment2(bipc::open_only, UTIL_SHARED_MEM_NAME);
  BOOST_LOG_TRIVIAL(info) << "opened shared_memory_object segment2: " << UTIL_SHARED_MEM_NAME
                          << ", segment2.free_memory: " << segment2.get_free_memory();

  atomic<uint64_t>* wraparound_sync = segment2.find<atomic<uint64_t>>("wraparound_sync").first;
  BOOST_LOG_TRIVIAL(info) << "wraparound_sync found, segment2.free_memory: " << segment2.get_free_memory();

  atomic<uint64_t>* startup_sync = segment2.find<atomic<uint64_t>>("startup_sync").first;
  BOOST_LOG_TRIVIAL(info) << "startup_sync found, segment2.free_memory: " << segment2.get_free_memory();

  const ReaderId id{reader_num};

  lshl::demux::core::DemuxReader<L, M> sub(id, span{*buffer}, message_count_sync, wraparound_sync);
  BOOST_LOG_TRIVIAL(info) << "DemuxReader created, segment1.free_memory: " << segment2.get_free_memory()
                          << ", segment2.free_memory: " << segment2.get_free_memory();

  startup_sync->fetch_or(id.mask());

  run_reader_loop(sub, msg_num);
  BOOST_LOG_TRIVIAL(info) << "DemuxReader completed, segment1.free_memory: " << segment2.get_free_memory()
                          << ", segment2.free_memory: " << segment2.get_free_memory();
}

template <size_t L, uint16_t M>
auto run_reader_loop(lshl::demux::core::DemuxReader<L, M>& sub, const uint64_t msg_num) noexcept(false) -> void {
  XXH64_util hash{};
  HDR_histogram_util histogram{};

  // consume the expected number of messages
  for (uint64_t i = 0; i < msg_num;) {
    const std::optional<const MarketDataUpdate*> read = sub.template next_object<MarketDataUpdate>();
    if (read.has_value()) {
      i += 1;
      const MarketDataUpdate* md = read.value();
      // track the latency
      histogram.record_value(calculate_latency(md->timestamp));
#ifndef NDEBUG
      BOOST_LOG_TRIVIAL(debug) << *md;
#endif
      // report progress
      if (i % REPORT_PROGRESS == 0) {
        BOOST_LOG_TRIVIAL(info) << "number of messages received: " << i;
      }
      // calculate the hash
      hash.update(md, sizeof(MarketDataUpdate));
    }
  }

  BOOST_LOG_TRIVIAL(info) << "reader sequence number: " << sub.message_count()
                          << ", XXH64_hash: " << XXH64_util::format(hash.digest());

  BOOST_LOG_TRIVIAL(info) << "message latency, ns:";
  histogram.print_report();
}

auto calculate_latency(const uint64_t x0) -> int64_t {
  const std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> now =
      std::chrono::steady_clock::now();
  const uint64_t x1 = static_cast<uint64_t>(now.time_since_epoch().count());
  return static_cast<int64_t>(x1 - x0);
}

}  // namespace lshl::demux::example

// NOLINTEND(misc-include-cleaner, cppcoreguidelines-pro-bounds-array-to-pointer-decay)