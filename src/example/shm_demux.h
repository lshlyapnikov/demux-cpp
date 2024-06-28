// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#ifndef LSHL_DEMUX_EXAMPLE_DEMUX_H
#define LSHL_DEMUX_EXAMPLE_DEMUX_H

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/log/trivial.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include "../main/demultiplexer.h"

namespace lshl::demux::example {

using std::size_t;
using std::uint16_t;

auto init_logging() noexcept -> void;

auto main_(std::span<char*> args) noexcept(false) -> int;

template <size_t L>
auto calculate_required_buffer_shared_mem_size() noexcept -> size_t;

template <size_t L, uint16_t M>
auto start_publisher(uint8_t total_subscriber_num, uint64_t msg_num) noexcept(false) -> void;

template <size_t L, uint16_t M>
auto run_publisher_loop(lshl::demux::DemultiplexerPublisher<L, M, false>& pub, uint64_t msg_num) noexcept(false)
    -> void;

template <class T, size_t L, uint16_t M>
[[nodiscard]] inline auto send_(lshl::demux::DemultiplexerPublisher<L, M, false>& pub, const T& md) noexcept -> bool;

template <size_t L, uint16_t M>
auto start_subscriber(uint8_t subscriber_num, uint64_t msg_num) noexcept(false) -> void;

template <size_t L, uint16_t M>
auto run_subscriber_loop(lshl::demux::DemultiplexerSubscriber<L, M>& sub, uint64_t msg_num) noexcept(false) -> void;

auto calculate_latency(uint64_t x0) -> int64_t;

struct ShmRemover {
  explicit ShmRemover(const char* name) : name_(name) {
    namespace bipc = boost::interprocess;
    const bool ok = bipc::shared_memory_object::remove(this->name_);
    if (ok) {
      BOOST_LOG_TRIVIAL(warning) << "startup: removed shared_memory_object: " << this->name_;
    } else {
      BOOST_LOG_TRIVIAL(info) << "startup: could not remove shared_memory_object: " << this->name_
                              << ", possible it did not exist.";
    }
  }

  ~ShmRemover() {
    namespace bipc = boost::interprocess;
    const bool ok = bipc::shared_memory_object::remove(this->name_);
    if (ok) {
      BOOST_LOG_TRIVIAL(info) << "shutdown: removed shared_memory_object: " << this->name_;
    } else {
      BOOST_LOG_TRIVIAL(error) << "shutdown: could not remove shared_memory_object: " << this->name_;
    }
  }

  ShmRemover(const ShmRemover&) = delete;                         // copy constructor
  auto operator=(const ShmRemover&) -> ShmRemover& = delete;      // copy assignment
  ShmRemover(ShmRemover&&) noexcept = delete;                     // move constructor
  auto operator=(ShmRemover&&) noexcept -> ShmRemover& = delete;  // move assignment

 private:
  const char* name_;
};

}  // namespace lshl::demux::example

#endif  // LSHL_DEMUX_EXAMPLE_DEMUX_H