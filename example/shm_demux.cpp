// #include <boost/log/core.hpp>
// #include <boost/interprocess/shared_memory_object.hpp>
#include "./shm_demux.h"
#include <array>
#include <atomic>
#include <boost/exception/all.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include "../src/demultiplexer.h"
#include "./market_data.h"
#include "shm_demux.h"

namespace bipc = boost::interprocess;

using lshl::demux::DemultiplexerPublisher;
using lshl::demux::DemultiplexerSubscriber;
using lshl::demux::SubscriberId;
using lshl::demux::example::init_logging;
using lshl::demux::example::PublisherResult;
using lshl::demux::example::ShmRemover;
using lshl::demux::example::SubscriberResult;

namespace lshl::demux::example {

constexpr char SHARED_MEM_NAME[] = "lshl_demux_example";

constexpr std::size_t PAGE_SIZE = 4096;
// total shared memory size, should be  a multiple of the page size (4kB on Linux). Because the operating system
// performs mapping operations over whole pages. So, you don't waste memory.
constexpr std::size_t SHARED_MEM_SIZE = 16 * PAGE_SIZE;
// names take up some space in the managed_shared_memory, 512 was enough
constexpr std::size_t SHARED_MEM_UTIL_SIZE = 512;
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

auto main(int argc, char* argv[]) -> int {
  using namespace lshl::demux::example;

  init_logging();

  if (argc != 4) {
    print_usage(argv[0]);
    return 1;
  }

  const std::string command(argv[1]);
  const uint16_t num16 = boost::lexical_cast<uint16_t>(argv[2]);
  if (num16 < 1 || num16 > std::numeric_limits<uint8_t>::max()) {
    print_usage(argv[0]);
    return 2;
  }
  const uint8_t num8 = static_cast<uint8_t>(num16);
  const uint64_t msg_num = boost::lexical_cast<uint64_t>(argv[3]);

  if (command == "pub") {
    BOOST_LOG_NAMED_SCOPE("pub");
    start_publisher<SHARED_MEM_SIZE, BUFFER_SIZE, MESSAGE_SIZE>(num8, msg_num);
  } else if (command == "sub") {
    BOOST_LOG_NAMED_SCOPE("sub");
    start_subscriber<BUFFER_SIZE, MESSAGE_SIZE>(num8, msg_num);
  } else {
    print_usage(argv[0]);
    return 3;
  }

  return 0;
}

namespace lshl::demux::example {

using std::array;
using std::atomic;
using std::size_t;
using std::uint16_t;

template <size_t SHM, size_t L, uint16_t M>
  requires(SHM > L && L >= M + 2 && M > 0)
auto start_publisher(const uint8_t total_subscriber_num, const uint64_t msg_num) -> PublisherResult {
  BOOST_LOG_TRIVIAL(info) << "start_publisher SHARED_MEM_NAME: " << SHARED_MEM_NAME << ", size: " << SHM << ", L: " << L
                          << ", M: " << M << ", total_subscriber_num: " << static_cast<int>(total_subscriber_num);

  const ShmRemover remover(SHARED_MEM_NAME);

  try {
    const uint64_t all_subs_mask = lshl::demux::SubscriberId::all_subscribers_mask(total_subscriber_num);

    bipc::managed_shared_memory segment(bipc::create_only, SHARED_MEM_NAME, SHM);
    BOOST_LOG_TRIVIAL(info) << "created shared_memory_object: " << SHARED_MEM_NAME
                            << ", free_memory: " << segment.get_free_memory();

    atomic<uint64_t>* message_count_sync = segment.construct<atomic<uint64_t>>("message_count_sync")(0);
    BOOST_LOG_TRIVIAL(info) << "message_count_sync allocated, free_memory: " << segment.get_free_memory();

    atomic<uint64_t>* wraparound_sync = segment.construct<atomic<uint64_t>>("wraparound_sync")(0);
    BOOST_LOG_TRIVIAL(info) << "wraparound_sync allocated, free_memory: " << segment.get_free_memory();

    array<uint8_t, L>* buffer = segment.construct<array<uint8_t, L>>("buffer")();
    BOOST_LOG_TRIVIAL(info) << "buffer allocated, free_memory: " << segment.get_free_memory();

    lshl::demux::DemultiplexerPublisher<L, M, false> pub(
        all_subs_mask, span{*buffer}, message_count_sync, wraparound_sync);
    BOOST_LOG_TRIVIAL(info) << "DemultiplexerPublisher created, free_memory: " << segment.get_free_memory();

    run_publisher(pub, msg_num);
    BOOST_LOG_TRIVIAL(info) << "DemultiplexerPublisher completed, free_memory: " << segment.get_free_memory();
  } catch (const boost::interprocess::interprocess_exception& e) {
    BOOST_LOG_TRIVIAL(error) << "interprocess_exception: " << boost::diagnostic_information(e);
    return PublisherResult::SharedMemoryCreateError;
  } catch (const boost::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Unexpected boost::exception: " << boost::diagnostic_information(e);
    return PublisherResult::UnexpectedError;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Unexpected std::exception: " << e.what();
    return PublisherResult::UnexpectedError;
  } catch (...) {
    BOOST_LOG_TRIVIAL(error) << "Unexpected error";
    return PublisherResult::UnexpectedError;
  }

  return PublisherResult::Success;
}

template <size_t L, uint16_t M>
[[nodiscard]] inline auto send_(lshl::demux::DemultiplexerPublisher<L, M, false>& pub, const span<uint8_t> md) -> bool {
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
        if (attempt % 1000 == 0) {
          BOOST_LOG_TRIVIAL(warning) << "one or more subscribers are lagging, wraparound is blocked, send attempt: "
                                     << attempt;
        }
        continue;
    }
  }
}

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
auto run_publisher(lshl::demux::DemultiplexerPublisher<L, M, false>& pub, const uint64_t msg_num) -> void {
  BOOST_LOG_TRIVIAL(info) << "sending " << msg_num << " md updates ...";

  MarketDataUpdate md;
  MarketDataUpdateGenerator md_gen;
  const size_t md_size = sizeof(MarketDataUpdate);

  for (uint64_t i = 0; i < msg_num; ++i) {
    md_gen.generate_market_data_update(&md);
    BOOST_LOG_TRIVIAL(debug) << md;
    const bool ok = send_(pub, std::span<uint8_t>{(uint8_t*)&md, md_size});
    if (!ok) {
      BOOST_LOG_TRIVIAL(error) << "dropping message, could not send: " << md;
    }
  }

  BOOST_LOG_TRIVIAL(info) << "publisher sequence number: " << pub.message_count();
}

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
auto start_subscriber(const uint8_t subscriber_num, const uint64_t msg_num) -> SubscriberResult {
  using lshl::demux::example::SHARED_MEM_NAME;
  using std::atomic;

  BOOST_LOG_TRIVIAL(info) << "subscriber SHARED_MEM_NAME: " << SHARED_MEM_NAME << ", L: " << L << ", M: " << M
                          << ", subscriber_num: " << static_cast<int>(subscriber_num);

  try {
    bipc::managed_shared_memory segment(bipc::open_only, SHARED_MEM_NAME);
    BOOST_LOG_TRIVIAL(info) << "opened shared_memory_object: " << SHARED_MEM_NAME
                            << ", free_memory: " << segment.get_free_memory();

    atomic<uint64_t>* message_count_sync = segment.find<atomic<uint64_t>>("message_count_sync").first;
    BOOST_LOG_TRIVIAL(info) << "message_count_sync found, free_memory: " << segment.get_free_memory();

    atomic<uint64_t>* wraparound_sync = segment.find<atomic<uint64_t>>("wraparound_sync").first;
    BOOST_LOG_TRIVIAL(info) << "wraparound_sync found, free_memory: " << segment.get_free_memory();

    array<uint8_t, L>* buffer = segment.find<array<uint8_t, L>>("buffer").first;
    BOOST_LOG_TRIVIAL(info) << "buffer found, free_memory: " << segment.get_free_memory();

    const SubscriberId id = SubscriberId::create(subscriber_num);

    lshl::demux::DemultiplexerSubscriber<L, M> sub(id, span{*buffer}, message_count_sync, wraparound_sync);
    BOOST_LOG_TRIVIAL(info) << "DemultiplexerSubscriber created, free_memory: " << segment.get_free_memory();

    run_subscriber(sub, msg_num);
    BOOST_LOG_TRIVIAL(info) << "DemultiplexerSubscriber completed, free_memory: " << segment.get_free_memory();

  } catch (const boost::interprocess::interprocess_exception& e) {
    BOOST_LOG_TRIVIAL(error) << "interprocess_exception: " << boost::diagnostic_information(e);
    return SubscriberResult::SharedMemoryOpenError;
  } catch (const boost::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Unexpected boost::exception: " << boost::diagnostic_information(e);
    return SubscriberResult::UnexpectedError;
  } catch (const std::exception& e) {
    BOOST_LOG_TRIVIAL(error) << "Unexpected std::exception: " << e.what();
    return SubscriberResult::UnexpectedError;
  } catch (...) {
    BOOST_LOG_TRIVIAL(error) << "Unexpected error";
    return SubscriberResult::UnexpectedError;
  }

  return SubscriberResult::Success;
}

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
auto run_subscriber(lshl::demux::DemultiplexerSubscriber<L, M>& sub, const uint64_t msg_num) -> void {
  assert(sub.message_count() == 0);
  assert(msg_num > 0);

  for (uint64_t i = 0; i < msg_num;) {
    const span<uint8_t> raw = sub.next();
    if (!raw.empty()) {
      i += 1;
      const MarketDataUpdate* md = (MarketDataUpdate*)raw.data();
      BOOST_LOG_TRIVIAL(debug) << *md;
    }
  }
}

}  // namespace lshl::demux::example