#include <atomic>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <iostream>
#include "./atomic_util.h"
#include "./domain.h"
#include "./sequencer.h"

using ShmSequencer::Sequencer;

namespace logging = boost::log;

void init_logging() {
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
}

auto main() -> int {
  init_logging();

  Sequencer sequencer(3);
  // sequencer.start();
  // sequencer.stop();

  BOOST_LOG_TRIVIAL(info) << "Starting sequencer: " << sequencer;
  BOOST_LOG_TRIVIAL(debug) << "...";
  BOOST_LOG_TRIVIAL(info) << "...";

  std::array<uint8_t, 8> arr{};
  arr.fill(0);

  for (auto a : arr) {
    BOOST_LOG_TRIVIAL(info) << "a: " << static_cast<size_t>(a);
  }

  // placement

  uint64_t* x = new (arr.data()) uint64_t();
  *x = 10;

  BOOST_LOG_TRIVIAL(info) << x;
  BOOST_LOG_TRIVIAL(info) << *x;

  for (auto a : arr) {
    BOOST_LOG_TRIVIAL(info) << "a: " << static_cast<size_t>(a);
  }

  struct A {
    int a[100];
  };
  struct B {
    int x, y;
  };
  struct C {
    uint64_t x;
  };
  struct D {
    uint64_t x, y;
  };
  struct E {
    uint64_t x;
    uint8_t y;
  };

  std::cout << "--------------------" << std::endl;
  {
    std::cout << std::boolalpha << "std::atomic<A> is lock free? " << std::atomic<A>{}.is_always_lock_free << '\n'
              << "std::atomic<B> is lock free? " << std::atomic<B>{}.is_always_lock_free << '\n';
    std::cout << std::boolalpha << "ax is lock free? " << std::atomic<uint64_t>{}.is_always_lock_free << '\n'
              << "std::atomic<C> is lock free? " << std::atomic<C>{}.is_always_lock_free << '\n'
              << "std::atomic<D> is lock free? " << std::atomic<D>{}.is_always_lock_free << '\n'
              << "std::atomic<E> is lock free? " << std::atomic<E>{}.is_always_lock_free << '\n';
  }
  std::cout << "--------------------" << std::endl;
  // std::array<uint8_t, 1024> buf{};
  // arr.fill(0);

  // auto x1 = new (buf.data()) ShmSequencer::DownstreamPacket<256>();

  std::atomic<uint64_t> actual(1);
  // actual.store(1024);
  const uint64_t y = ShmSequencer::wait_and_acquire(actual, 1024);
  std::cout << "y: " << y << std::endl;

  return 0;
}
