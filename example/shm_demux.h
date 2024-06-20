#ifndef LSHL_DEMUX_EXAMPLE_DEMUX_H
#define LSHL_DEMUX_EXAMPLE_DEMUX_H

#include <xxhash.h>
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
auto run_publisher_loop(lshl::demux::DemultiplexerPublisher<L, M, false>& pub, uint64_t msg_num) noexcept(false)
    -> void;

template <class T, size_t L, uint16_t M>
[[nodiscard]] inline auto send_(lshl::demux::DemultiplexerPublisher<L, M, false>& pub, const T& md) noexcept -> bool;

template <size_t L, uint16_t M>
auto start_subscriber(uint8_t subscriber_num, uint64_t msg_num) noexcept(false) -> void;

template <size_t L, uint16_t M>
auto run_subscriber_loop(lshl::demux::DemultiplexerSubscriber<L, M>& sub, uint64_t msg_num) noexcept(false) -> void;

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

  ShmRemover(const ShmRemover&) = delete;                         // copy constructor
  auto operator=(const ShmRemover&) -> ShmRemover& = delete;      // copy assignment
  ShmRemover(ShmRemover&&) noexcept = delete;                     // move constructor
  auto operator=(ShmRemover&&) noexcept -> ShmRemover& = delete;  // move assignment

 private:
  const char* name_;
};

struct XXH64_state_remover {
  explicit XXH64_state_remover(XXH64_state_t* state) : state_(state){};
  ~XXH64_state_remover() { XXH64_freeState(this->state_); }

  XXH64_state_remover(const XXH64_state_remover&) = delete;                         // copy constructor
  auto operator=(const XXH64_state_remover&) -> XXH64_state_remover& = delete;      // copy assignment
  XXH64_state_remover(XXH64_state_remover&&) noexcept = delete;                     // move constructor
  auto operator=(XXH64_state_remover&&) noexcept -> XXH64_state_remover& = delete;  // move assignment

 private:
  XXH64_state_t* const state_;
};

}  // namespace lshl::demux::example

#endif  // LSHL_DEMUX_EXAMPLE_DEMUX_H