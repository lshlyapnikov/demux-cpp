// #include <boost/log/core.hpp>
// #include <boost/interprocess/shared_memory_object.hpp>
#include "./shm_demux.h"
#include <array>
#include <atomic>
#include <boost/exception/all.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
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
#include "./domain.h"

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

auto main(int argc, char* argv[]) -> int {
  using namespace lshl::demux::example;

  init_logging();

  BOOST_LOG_TRIVIAL(info) << "argc: " << argc << ", argv[0]: " << argv[0];

  if (argc == 1) {
    // TODO(Leo): read `--total-number-of-subscribers` argument
    start_publisher<SHARED_MEM_SIZE, BUFFER_SIZE, MESSAGE_SIZE>(3);
  } else if (argc == 2) {
    // TODO(Leo): read `--subscriber-number` argument
    std::istringstream iss(argv[1]);
    uint8_t subscriber_num;
    if (iss >> subscriber_num) {
      start_subscriber<BUFFER_SIZE, MESSAGE_SIZE>(subscriber_num);
    } else {
      BOOST_LOG_TRIVIAL(info) << "invalid subscriber number argument: " << argv[1];
    }
  } else {
    BOOST_LOG_TRIVIAL(error)
        << "Invalid number of arguments, expected 0 or exactly 1 argument which specifies subscriber number\n";
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
auto start_publisher(const uint8_t total_subscriber_num) -> PublisherResult {
  BOOST_LOG_TRIVIAL(info) << "start_publisher SHARED_MEM_NAME: " << SHARED_MEM_NAME << ", size: " << SHM << ", L: " << L
                          << ", M: " << M << ", total_subscriber_num: " << total_subscriber_num;

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
  requires(L >= M + 2 && M > 0)
auto start_subscriber(const uint8_t subscriber_num) -> SubscriberResult {
  BOOST_LOG_TRIVIAL(info) << "subscriber SHARED_MEM_NAME: " << SHARED_MEM_NAME << ", L: " << L << ", M: " << M
                          << ", subscriber_num: " << subscriber_num;

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
}  // namespace lshl::demux::example