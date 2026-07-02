// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#define XXH_INLINE_ALL  // <xxhash.h>

#include "./shm_demux.h"
#include <array>
#include <atomic>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/expressions.hpp>  // NOLINT(misc-include-cleaner)
#include <chrono>
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
#include "../core/demux_reader.h"
#include "../core/demux_writer.h"
#include "../core/reader_id.h"
#include "../util/atomic_util.h"
#include "../util/boost_log_util.h"
#include "../util/hdr_histogram_util.h"
#include "../util/operators.h"
#include "../util/shm_manager.h"
#include "../util/shm_remover.h"
#include "../util/xxhash_util.h"
#include "./market_data.h"

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
  constexpr int ERROR = 100;
  try {
    const auto args = std::span<char*>(argv, static_cast<size_t>(argc));
    return lshl::demux::example::main_(args);
  } catch (const boost::exception& e) {
    LOG_ERROR << "boost::exception: " << boost::diagnostic_information(e);
    return ERROR;
  } catch (const std::exception& e) {
    LOG_ERROR << "std::exception: " << e.what();
    return ERROR;
  } catch (...) {
    LOG_ERROR << "unexpected exception";
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
using lshl::demux::util::ShmRemover;
using lshl::demux::util::XXH64_util;
using std::array;
using std::atomic;
using std::size_t;
using std::span;
using std::uint16_t;
using std::vector;

auto main_(const span<char*> args) noexcept(false) -> int {
  constexpr int ERROR = 200;
  constexpr size_t EXPECTED_ARG_NUM = 5;

  init_logging();

  if (args.size() != EXPECTED_ARG_NUM) {
    print_usage(args[0]);
    return ERROR;
  }

  const std::string command(args[1]);
  const auto num16 = boost::lexical_cast<uint16_t>(args[2]);
  if (num16 < 1 || num16 > std::numeric_limits<std::uint8_t>::max()) {
    print_usage(args[0]);
    return ERROR;
  }
  const auto num8 = static_cast<uint8_t>(num16);
  const auto msg_num = boost::lexical_cast<uint64_t>(args[3]);
  const auto zero_copy = std::string("true") == args[4];

  // TODO(Leonid): pass it as a command line argument
  const std::string shared_memory_name = "lshl_demux_buf";

  if (command == "writer") {
    start_writer<BUFFER_SIZE, MAX_MESSAGE_SIZE, MAX_READER_NUM>(shared_memory_name, num8, msg_num, zero_copy);
  } else if (command == "reader") {
    start_reader<BUFFER_SIZE, MAX_MESSAGE_SIZE>(shared_memory_name, num8, msg_num);
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

template <size_t L, uint16_t M, size_t R>
auto start_writer(
    const string& shared_memory_name,
    [[maybe_unused]] const uint8_t total_reader_num,
    [[maybe_unused]] const uint64_t msg_num,
    [[maybe_unused]] bool zero_copy
) noexcept(false) -> void {
  constexpr size_t SHM_SIZE = lshl::demux::util::calculate_shared_mem_size<L, R>();
  util::ShmManager<L, R> shm_manager{bipc::create_only, shared_memory_name, SHM_SIZE};

  util::ShmWriterData<L>* writer_data = shm_manager.construct_shm_writer_data();
  util::ShmReaderData<R>* reader_data = shm_manager.construct_shm_reader_data();

  span<uint8_t, L> buffer = writer_data->buffer.value;
  atomic<uint64_t>* downstream_sequence = &writer_data->downstream_sequence.value;
  const vector<const atomic<uint64_t>*>& upstream_sequences =
      util::to_const_pointer_vector(util::to_upstream_sequence_pointers(reader_data->upstream_sequences));

  DemuxWriter<L, M, false> writer(buffer, downstream_sequence, upstream_sequences);

  const atomic<size_t>* reader_count = &reader_data->active_reader_count.value;

  LOG_INFO << "waiting for all readers: " << total_reader_num << "...";
  util::wait_for_count<size_t>(reader_count, total_reader_num);
  LOG_INFO << "all readers connected";

  // if (zero_copy) {
  //   run_writer_loop_zero_copy(&writer, msg_num);
  // } else {
  //   run_writer_loop(&writer, msg_num);
  // }
  // LOG_INFO << "DemuxWriter completed, segment1.free_memory: " << segment1.get_free_memory()
  //          << ", segment2.free_memory: " << segment2.get_free_memory();
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
          LOG_WARNING << "one or more readers are lagging, wraparound is blocked, write attempt: " << attempt
                      << ", writer sequence: " << writer->message_count();
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

template <size_t L, uint16_t M>
auto start_reader(
    [[maybe_unused]] const string& shared_memory_name,
    [[maybe_unused]] const uint8_t reader_num,
    [[maybe_unused]] const uint64_t msg_num
) noexcept(false) -> void {
  using std::atomic;

  // LOG_INFO << "reader BUFFER_SHARED_MEM_NAME: " << BUFFER_SHARED_MEM_NAME.data() << ", L: " << L << ", M: " << M
  //          << ", reader_num: " << static_cast<int>(reader_num);

  // // read-only segment for the circular buffer and message counter
  // // NOLINTNEXTLINE(misc-include-cleaner)
  // bipc::managed_shared_memory segment1(bipc::open_read_only, BUFFER_SHARED_MEM_NAME.data());
  // LOG_INFO << "opened shared_memory_object segment1: " << BUFFER_SHARED_MEM_NAME.data()
  //          << ", segment1.free_memory: " << segment1.get_free_memory();

  // array<uint8_t, L>* buffer = segment1.find<array<uint8_t, L>>("buffer").first;
  // LOG_INFO << "buffer found, segment1.free_memory: " << segment1.get_free_memory();

  // atomic<uint64_t>* message_count_sync = segment1.find<atomic<uint64_t>>("message_count_sync").first;
  // LOG_INFO << "message_count_sync found, segment1.free_memory: " << segment1.get_free_memory();

  // read-write segment for atomic variables
  // NOLINTNEXTLINE(misc-include-cleaner)
  // bipc::managed_shared_memory segment2(bipc::open_only, UTIL_SHARED_MEM_NAME.data());
  // LOG_INFO << "opened shared_memory_object segment2: " << UTIL_SHARED_MEM_NAME.data()
  //          << ", segment2.free_memory: " << segment2.get_free_memory();

  // atomic<uint64_t>* wraparound_sync = segment2.find<atomic<uint64_t>>("wraparound_sync").first;
  // LOG_INFO << "wraparound_sync found, segment2.free_memory: " << segment2.get_free_memory();

  // atomic<uint64_t>* startup_sync = segment2.find<atomic<uint64_t>>("startup_sync").first;
  // LOG_INFO << "startup_sync found, segment2.free_memory: " << segment2.get_free_memory();

  // const ReaderId id{reader_num};

  // DemuxReader<L, M> reader(id, span{*buffer}, message_count_sync, wraparound_sync);
  // LOG_INFO << "DemuxReader created, segment1.free_memory: " << segment2.get_free_memory()
  //          << ", segment2.free_memory: " << segment2.get_free_memory();

  // startup_sync->fetch_or(id.mask());

  // run_reader_loop(&reader, msg_num);
  // LOG_INFO << "DemuxReader completed, segment1.free_memory: " << segment2.get_free_memory()
  //          << ", segment2.free_memory: " << segment2.get_free_memory();
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
