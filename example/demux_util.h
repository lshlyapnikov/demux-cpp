#ifndef DEMUX_UTIL_H
#define DEMUX_UTIL_H

#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

namespace lshl::demux::example {

namespace bipc = boost::interprocess;

// NOLINTNEXTLINE(shadow)
enum class PublisherResult { Success, SharedMemoryCreateError, UnexpectedError };

// NOLINTNEXTLINE(shadow)
enum class SubscriberResult { Success, SharedMemoryOpenError, UnexpectedError };

template <size_t SHM, uint16_t M, bool B>
  requires(SHM >= M + 2 + 8 + 8 && M > 0)
auto publisher(const char* shared_mem_name, const uint8_t total_subscriber_num) -> PublisherResult;

auto subscriber(const SubscriberId& id) -> SubscriberResult;

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

#endif  // DEMUX_UTIL_H