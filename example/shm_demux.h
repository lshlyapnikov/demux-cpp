#ifndef LSHL_DEMUX_EXAMPLE_DEMUX_H
#define LSHL_DEMUX_EXAMPLE_DEMUX_H

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <iostream>
#include "../src/demultiplexer.h"

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
auto start_publisher(const uint8_t total_subscriber_num, const uint64_t msg_num) -> PublisherResult;

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
auto run_publisher(lshl::demux::DemultiplexerPublisher<L, M, false>& pub, const uint64_t msg_num) -> void;

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
auto start_subscriber(const uint8_t subscriber_num, const uint64_t msg_num) -> SubscriberResult;

template <size_t L, uint16_t M>
  requires(L >= M + 2 && M > 0)
auto run_subscriber(lshl::demux::DemultiplexerSubscriber<L, M>& sub, const uint64_t msg_num) -> void;

auto init_logging() -> void {
  namespace logging = boost::log;
  namespace expr = boost::log::expressions;
  namespace keywords = boost::log::keywords;
  namespace sinks = boost::log::sinks;

  // Add Scope
  logging::add_common_attributes();
  logging::core::get()->add_global_attribute("Scope", logging::attributes::named_scope());

  // Customize output format and enable auto_flush
  boost::shared_ptr<sinks::sink> console_sink = logging::add_console_log(
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

struct ShmRemover {
  ShmRemover(const char* name) : name_(name) {
    BOOST_LOG_NAMED_SCOPE("startup");
    const bool ok = bipc::shared_memory_object::remove(this->name_);
    if (ok) {
      BOOST_LOG_TRIVIAL(warning) << "removed shared_memory_object: " << this->name_;
    } else {
      BOOST_LOG_TRIVIAL(info) << "could not remove shared_memory_object: " << this->name_
                              << ", possible it did not exist.";
    }
  }

  ~ShmRemover() {
    BOOST_LOG_NAMED_SCOPE("shutdown");
    const bool ok = bipc::shared_memory_object::remove(this->name_);
    if (ok) {
      BOOST_LOG_TRIVIAL(info) << "removed shared_memory_object: " << this->name_;
    } else {
      BOOST_LOG_TRIVIAL(error) << "could not remove shared_memory_object: " << this->name_;
    }
  }

 private:
  const char* name_;
};

}  // namespace lshl::demux::example

#endif  // LSHL_DEMUX_EXAMPLE_DEMUX_H