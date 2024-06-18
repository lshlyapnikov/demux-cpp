#ifndef LSHL_DEMUX_EXAMPLE_DEMUX_H
#define LSHL_DEMUX_EXAMPLE_DEMUX_H

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

namespace lshl::demux::example {

namespace bipc = boost::interprocess;

using std::size_t;
using std::uint16_t;

// NOLINTNEXTLINE(shadow)
enum class PublisherResult { Success, SharedMemoryCreateError, UnexpectedError };

// NOLINTNEXTLINE(shadow)
enum class SubscriberResult { Success, SharedMemoryOpenError, UnexpectedError };

template <size_t SHM, size_t L, uint16_t M>
  requires(SHM > L && L >= M + 2 && M > 0)
auto start_publisher(const uint8_t total_subscriber_num) -> PublisherResult;

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
auto start_subscriber(const uint8_t subscriber_num) -> SubscriberResult;

auto init_logging() -> void {
  namespace logging = boost::log;
  // NOLINTNEXTLINE(misc-include-cleaner)
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
}

struct ShmRemover {
  ShmRemover(const char* name) : name_(name) {
    const bool ok = bipc::shared_memory_object::remove(this->name_);
    if (ok) {
      BOOST_LOG_TRIVIAL(warning) << "[startup] removed shared_memory_object: " << this->name_;
    } else {
      BOOST_LOG_TRIVIAL(info) << "[startup] could not remove shared_memory_object: " << this->name_
                              << ", possible it did not exist.";
    }
  }

  ~ShmRemover() {
    const bool ok = bipc::shared_memory_object::remove(this->name_);
    if (ok) {
      BOOST_LOG_TRIVIAL(info) << "[shutdown] removed shared_memory_object: " << this->name_;
    } else {
      BOOST_LOG_TRIVIAL(error) << "[shutdown] could not remove shared_memory_object: " << this->name_;
    }
  }

 private:
  const char* name_;
};

}  // namespace lshl::demux::example

#endif  // LSHL_DEMUX_EXAMPLE_DEMUX_H