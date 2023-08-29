#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <iostream>

#include "./schema_generated.h"
#include "./sequencer.h"

using ShmSequencer::Sequencer;

namespace logging = boost::log;

void init_logging() {
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
}

// auto test(const size_t size) -> uint8_t* {
//   return new uint8_t[size];
// }

auto main() -> int {
  init_logging();

  Sequencer sequencer(3);
  // sequencer.start();
  // sequencer.stop();

  BOOST_LOG_TRIVIAL(info) << "Starting sequencer: " << sequencer;
  BOOST_LOG_TRIVIAL(debug) << "...";
  BOOST_LOG_TRIVIAL(info) << "...";

  std::array<uint8_t, 8> arr;
  arr.fill(0);

  for (auto a : arr) {
    BOOST_LOG_TRIVIAL(info) << "a: " << a;
  }

  // placement
  uint64_t* x = new (arr.data()) uint64_t();
  *x = 10;

  BOOST_LOG_TRIVIAL(info) << x;
  BOOST_LOG_TRIVIAL(info) << *x;
  for (auto a : arr) {
    BOOST_LOG_TRIVIAL(info) << "a: " << a;
  }

  return 0;
}