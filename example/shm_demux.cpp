// #include <boost/log/core.hpp>
// #include <boost/interprocess/shared_memory_object.hpp>
#include <array>
#include <atomic>
#include <boost/exception/all.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <stdexcept>
#include "../src/demultiplexer.h"
#include "./demux_util.h"

namespace bipc = boost::interprocess;

using lshl::demux::DemultiplexerPublisher;
using lshl::demux::DemultiplexerSubscriber;
using lshl::demux::SubscriberId;
using lshl::demux::example::init_logging;
using lshl::demux::example::PublisherResult;
using lshl::demux::example::ShmRemover;
using lshl::demux::example::SubscriberResult;

constexpr char SHARED_MEM_NAME[] = "lshl_demux_example";
constexpr std::size_t SHARED_MEM_SIZE = 65536;
constexpr std::uint16_t MESSAGE_SIZE = 256;

auto main() -> int {
  using namespace lshl::demux::example;

  init_logging();

  publisher<SHARED_MEM_SIZE, MESSAGE_SIZE, false>(SHARED_MEM_NAME, 3);

  return 0;
}

namespace lshl::demux::example {
// SHM the size of the shared memory should be  a multiple of the page size (4kB on Linux). Because the operating system
// performs mapping operations over whole pages. So, you don't waste memory.
template <size_t SHM, uint16_t M, bool B>
  requires(SHM >= M + 2 + 8 + 8 && M > 0)
auto publisher(const char* shared_mem_name, const uint8_t total_subscriber_num) -> PublisherResult {
  using std::array;
  using std::atomic;
  using std::size_t;

  // names take up some space in the managed_shared_memory
  constexpr std::size_t L = SHM - 512;

  BOOST_LOG_TRIVIAL(info) << "publisher shared_memory_name: " << shared_mem_name << ", size: " << SHM << ", L: " << L
                          << ", M: " << M << ", total_subscriber_num: " << total_subscriber_num;

  const ShmRemover remover(shared_mem_name);

  try {
    const uint64_t all_subs_mask = lshl::demux::SubscriberId::all_subscribers_mask(total_subscriber_num);

    bipc::managed_shared_memory segment(bipc::create_only, shared_mem_name, SHM);
    BOOST_LOG_TRIVIAL(info) << "created shared_memory_object: " << shared_mem_name
                            << ", free_memory: " << segment.get_free_memory();

    atomic<uint64_t>* message_count_sync = segment.construct<atomic<uint64_t>>("message_count_sync")(0);
    BOOST_LOG_TRIVIAL(info) << "message_count_sync allocated"
                            << ", free_memory: " << segment.get_free_memory();

    atomic<uint64_t>* wraparound_sync = segment.construct<atomic<uint64_t>>("wraparound_sync")(0);
    BOOST_LOG_TRIVIAL(info) << "wraparound_sync allocated"
                            << ", free_memory: " << segment.get_free_memory();

    array<uint8_t, L>* buffer = segment.construct<array<uint8_t, L>>("buffer")();
    BOOST_LOG_TRIVIAL(info) << "buffer allocated"
                            << ", free_memory: " << segment.get_free_memory();

    lshl::demux::DemultiplexerPublisher<L, M, false> pub(
        all_subs_mask, span{*buffer}, message_count_sync, wraparound_sync);
    BOOST_LOG_TRIVIAL(info) << "DemultiplexerPublisher allocated"
                            << ", free_memory: " << segment.get_free_memory();

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

auto subscriber(const SubscriberId& id) -> SubscriberResult {
  assert(id.mask() > 0);

  // bipc::managed_shared_memory segment(bipc::open_only, shared_mem_name);

  return SubscriberResult::Success;
}
}  // namespace lshl::demux::example