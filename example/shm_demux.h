#ifndef LSHL_DEMUX_EXAMPLE_DEMUX_H
#define LSHL_DEMUX_EXAMPLE_DEMUX_H

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/trivial.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include "../src/demultiplexer.h"

namespace lshl::demux::example {

using std::size_t;
using std::uint16_t;

auto main_(std::span<char*> args) noexcept(false) -> int;

template <size_t SHM, size_t L, uint16_t M>
auto start_publisher(uint8_t total_subscriber_num, uint64_t msg_num) noexcept(false) -> void;

template <size_t L, uint16_t M>
auto run_publisher_loop(lshl::demux::DemultiplexerPublisher<L, M, false>& pub, uint64_t msg_num) noexcept -> void;

template <size_t L, uint16_t M>
[[nodiscard]] inline auto send_(lshl::demux::DemultiplexerPublisher<L, M, false>& pub, span<uint8_t> md) noexcept
    -> bool;

template <size_t L, uint16_t M>
auto start_subscriber(uint8_t subscriber_num, uint64_t msg_num) noexcept(false) -> void;

template <size_t L, uint16_t M>
auto run_subscriber_loop(lshl::demux::DemultiplexerSubscriber<L, M>& sub, uint64_t msg_num) noexcept -> void;

auto init_logging() noexcept -> void;

struct ShmRemover {
  explicit ShmRemover(const char* name) : name_(name) {
    namespace bipc = boost::interprocess;
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
    namespace bipc = boost::interprocess;
    BOOST_LOG_NAMED_SCOPE("shutdown");
    const bool ok = bipc::shared_memory_object::remove(this->name_);
    if (ok) {
      BOOST_LOG_TRIVIAL(info) << "removed shared_memory_object: " << this->name_;
    } else {
      BOOST_LOG_TRIVIAL(error) << "could not remove shared_memory_object: " << this->name_;
    }
  }

  ShmRemover(const ShmRemover&) = delete;                          // copy constructor
  auto operator=(const ShmRemover&) -> ShmRemover& = delete;       // copy assignment
  ShmRemover(ShmRemover&&) noexcept = default;                     // move constructor
  auto operator=(ShmRemover&&) noexcept -> ShmRemover& = default;  // move assignment

 private:
  const char* name_;
};

}  // namespace lshl::demux::example

#endif  // LSHL_DEMUX_EXAMPLE_DEMUX_H