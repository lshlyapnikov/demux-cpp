// NOLINTBEGIN(misc-include-cleaner)

#include "./shm_demux.h"
#include <array>
#include <atomic>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <thread>
#include "../src/demultiplexer.h"
#include "../src/domain.h"
#include "./market_data.h"

namespace lshl::demux::example {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
constexpr char SHARED_MEM_NAME[] = "lshl_demux_example";

constexpr int REPORT_PROGRESS = 1000000;

constexpr std::size_t PAGE_SIZE = 4096;
// total shared memory size, should be  a multiple of the page size (4kB on Linux). Because the operating system
// performs mapping operations over whole pages. So, you don't waste memory.
constexpr std::size_t SHARED_MEM_SIZE = 16 * PAGE_SIZE;
// names take up some space in the managed_shared_memory, 512 was enough
constexpr std::size_t SHARED_MEM_UTIL_SIZE = 512 + 32;
constexpr std::size_t BUFFER_SIZE = SHARED_MEM_SIZE - SHARED_MEM_UTIL_SIZE;
constexpr std::uint16_t MESSAGE_SIZE = 256;

}  // namespace lshl::demux::example

auto print_usage(const char* prog) -> void {
  std::cerr << "Usage: " << prog << " [pub <total-number-of-subscribers> <number-of-messages-to-send>] "
            << " | [sub <the-subscriber-number> <number-of-messages-to-receive>]\n"
            << "\twhere <total-number-of-subscribers> and <the-subscriber-number> are within the interval [1, "
            << static_cast<int>(std::numeric_limits<uint8_t>::max()) << "] (uint8_t)\n"
            << "\t<number-of-messages-to-send> and <number-of-messages-to-receive> are within the interval [1, "
            << std::numeric_limits<uint64_t>::max() << "] (uint64_t)\n";
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

namespace bipc = boost::interprocess;

using lshl::demux::DemultiplexerPublisher;
using lshl::demux::DemultiplexerSubscriber;
using lshl::demux::SubscriberId;
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
  if (num16 < 1 || num16 > std::numeric_limits<uint8_t>::max()) {
    print_usage(args[0]);
    return ERROR;
  }
  const auto num8 = static_cast<uint8_t>(num16);
  const auto msg_num = boost::lexical_cast<uint64_t>(args[3]);

  if (command == "pub") {
    BOOST_LOG_NAMED_SCOPE("pub");
    start_publisher<SHARED_MEM_SIZE, BUFFER_SIZE, MESSAGE_SIZE>(num8, msg_num);
  } else if (command == "sub") {
    BOOST_LOG_NAMED_SCOPE("sub");
    start_subscriber<BUFFER_SIZE, MESSAGE_SIZE>(num8, msg_num);
  } else {
    print_usage(args[0]);
    return ERROR;
  }

  return 0;
}

auto init_logging() noexcept -> void {
  namespace logging = boost::log;
  namespace expr = boost::log::expressions;
  namespace keywords = boost::log::keywords;
  namespace sinks = boost::log::sinks;

  // Add Scope
  logging::add_common_attributes();
  logging::core::get()->add_global_attribute("Scope", logging::attributes::named_scope());

  // Customize output format and enable auto_flush
  const boost::shared_ptr<sinks::sink> console_sink = logging::add_console_log(
      std::cout,
      keywords::auto_flush = true,
      keywords::format =
          (expr::stream << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << " ["
                        << logging::trivial::severity << "] ["
                        << expr::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID") << "] ["
                        << expr::format_named_scope("Scope", keywords::format = "%n", keywords::depth = 2) << "] "
                        << expr::smessage));

  // Configure logging severity
  // NOLINTNEXTLINE(misc-include-cleaner)
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
}

template <size_t SHM, size_t L, uint16_t M>
auto start_publisher(const uint8_t total_subscriber_num, const uint64_t msg_num) noexcept(false) -> void {
  BOOST_LOG_TRIVIAL(info) << "start_publisher SHARED_MEM_NAME: " << SHARED_MEM_NAME << ", size: " << SHM << ", L: " << L
                          << ", M: " << M << ", total_subscriber_num: " << static_cast<int>(total_subscriber_num);

  const ShmRemover remover(SHARED_MEM_NAME);

  const uint64_t all_subs_mask = SubscriberId::all_subscribers_mask(total_subscriber_num);

  bipc::managed_shared_memory segment(bipc::create_only, SHARED_MEM_NAME, SHM);
  BOOST_LOG_TRIVIAL(info) << "created shared_memory_object: " << SHARED_MEM_NAME
                          << ", free_memory: " << segment.get_free_memory();

  atomic<uint64_t>* message_count_sync = segment.construct<atomic<uint64_t>>("message_count_sync")(0);
  BOOST_LOG_TRIVIAL(info) << "message_count_sync allocated, free_memory: " << segment.get_free_memory();

  atomic<uint64_t>* wraparound_sync = segment.construct<atomic<uint64_t>>("wraparound_sync")(0);
  BOOST_LOG_TRIVIAL(info) << "wraparound_sync allocated, free_memory: " << segment.get_free_memory();

  array<uint8_t, L>* buffer = segment.construct<array<uint8_t, L>>("buffer")();
  BOOST_LOG_TRIVIAL(info) << "buffer allocated, free_memory: " << segment.get_free_memory();

  atomic<uint64_t>* startup_sync = segment.construct<atomic<uint64_t>>("startup_sync")(0);
  BOOST_LOG_TRIVIAL(info) << "startup_sync allocated, free_memory: " << segment.get_free_memory();

  lshl::demux::DemultiplexerPublisher<L, M, false> pub(
      all_subs_mask, span{*buffer}, message_count_sync, wraparound_sync);
  BOOST_LOG_TRIVIAL(info) << "DemultiplexerPublisher created, free_memory: " << segment.get_free_memory();

  BOOST_LOG_TRIVIAL(info) << "waiting for all subscribers ...";
  while (true) {
    const uint64_t x = startup_sync->load();
    if (x == all_subs_mask) {
      break;
    } else {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(1s);
    }
  }
  BOOST_LOG_TRIVIAL(info) << "all subscribers connected";

  run_publisher_loop(pub, msg_num);
  BOOST_LOG_TRIVIAL(info) << "DemultiplexerPublisher completed, free_memory: " << segment.get_free_memory();
}

template <size_t L, uint16_t M>
auto run_publisher_loop(lshl::demux::DemultiplexerPublisher<L, M, false>& pub, const uint64_t msg_num) noexcept
    -> void {
  BOOST_LOG_TRIVIAL(info) << "sending " << msg_num << " md updates ...";

  MarketDataUpdate md{};
  MarketDataUpdateGenerator md_gen{};
  const size_t md_size = sizeof(MarketDataUpdate);

  for (uint64_t i = 0; i < msg_num; ++i) {
    md_gen.generate_market_data_update(&md);
    BOOST_LOG_TRIVIAL(debug) << md;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const span<uint8_t> raw{reinterpret_cast<uint8_t*>(&md), md_size};
    // send will copy the raw bytes into the circular buffer, we can re-use the md object
    const bool ok = send_(pub, raw);
    if (!ok) {
      BOOST_LOG_TRIVIAL(error) << "dropping message, could not send: " << md;
    }
    if (i % REPORT_PROGRESS == 0) {
      BOOST_LOG_TRIVIAL(info) << "number of messages sent: " << i;
    }
  }

  BOOST_LOG_TRIVIAL(info) << "publisher sequence number: " << pub.message_count();
}

template <size_t L, uint16_t M>
[[nodiscard]] inline auto send_(lshl::demux::DemultiplexerPublisher<L, M, false>& pub, const span<uint8_t> md) noexcept
    -> bool {
  int attempt = 0;
  while (true) {
    const SendResult result = pub.send(md);
    switch (result) {
      case SendResult::Success:
        return true;
      case SendResult::Error:
        return false;
      case SendResult::Repeat:
        attempt += 1;
        if (attempt % REPORT_PROGRESS == 0) {
          BOOST_LOG_TRIVIAL(warning) << "one or more subscribers are lagging, wraparound is blocked, send attempt: "
                                     << attempt << ", publisher sequence: " << pub.message_count();
        }
        continue;
    }
  }
}

template <size_t L, uint16_t M>
auto start_subscriber(const uint8_t subscriber_num, const uint64_t msg_num) noexcept(false) -> void {
  using lshl::demux::example::SHARED_MEM_NAME;
  using std::atomic;

  BOOST_LOG_TRIVIAL(info) << "subscriber SHARED_MEM_NAME: " << SHARED_MEM_NAME << ", L: " << L << ", M: " << M
                          << ", subscriber_num: " << static_cast<int>(subscriber_num);

  bipc::managed_shared_memory segment(bipc::open_only, SHARED_MEM_NAME);
  BOOST_LOG_TRIVIAL(info) << "opened shared_memory_object: " << SHARED_MEM_NAME
                          << ", free_memory: " << segment.get_free_memory();

  atomic<uint64_t>* message_count_sync = segment.find<atomic<uint64_t>>("message_count_sync").first;
  BOOST_LOG_TRIVIAL(info) << "message_count_sync found, free_memory: " << segment.get_free_memory();

  atomic<uint64_t>* wraparound_sync = segment.find<atomic<uint64_t>>("wraparound_sync").first;
  BOOST_LOG_TRIVIAL(info) << "wraparound_sync found, free_memory: " << segment.get_free_memory();

  array<uint8_t, L>* buffer = segment.find<array<uint8_t, L>>("buffer").first;
  BOOST_LOG_TRIVIAL(info) << "buffer found, free_memory: " << segment.get_free_memory();

  atomic<uint64_t>* startup_sync = segment.find<atomic<uint64_t>>("startup_sync").first;
  BOOST_LOG_TRIVIAL(info) << "startup_sync found, free_memory: " << segment.get_free_memory();

  const SubscriberId id = SubscriberId::create(subscriber_num);

  lshl::demux::DemultiplexerSubscriber<L, M> sub(id, span{*buffer}, message_count_sync, wraparound_sync);
  BOOST_LOG_TRIVIAL(info) << "DemultiplexerSubscriber created, free_memory: " << segment.get_free_memory();

  startup_sync->fetch_or(id.mask());

  run_subscriber_loop(sub, msg_num);
  BOOST_LOG_TRIVIAL(info) << "DemultiplexerSubscriber completed, free_memory: " << segment.get_free_memory();
}

template <size_t L, uint16_t M>
auto run_subscriber_loop(lshl::demux::DemultiplexerSubscriber<L, M>& sub, const uint64_t msg_num) noexcept -> void {
  assert(sub.message_count() == 0);
  assert(msg_num > 0);

  for (uint64_t i = 0; i < msg_num;) {
    const span<uint8_t> raw = sub.next();
    if (!raw.empty()) {
      i += 1;
      // do NOT keep the reference to the object retrieved from the circular buffer, it may get overriden!!!
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      const MarketDataUpdate* md = reinterpret_cast<MarketDataUpdate*>(raw.data());
      BOOST_LOG_TRIVIAL(debug) << *md;
      if (i % REPORT_PROGRESS == 0) {
        BOOST_LOG_TRIVIAL(info) << "number of messages received: " << i;
      }
    }
  }

  BOOST_LOG_TRIVIAL(info) << "subscriber sequence number: " << sub.message_count();
}

}  // namespace lshl::demux::example

// NOLINTEND(misc-include-cleaner)