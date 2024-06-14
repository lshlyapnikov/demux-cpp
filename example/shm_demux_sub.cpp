// #include <boost/log/core.hpp> // NOLINT(misc-include-cleaner)
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include "./demux_util.h"

auto main() -> int {
  demux::init_logging();
  BOOST_LOG_TRIVIAL(info) << "shm_demux_sub ..." << '\n';

  return 0;
}