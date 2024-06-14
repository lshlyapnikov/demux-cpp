#ifndef DEMUX_UTIL_H
#define DEMUX_UTIL_H

#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

namespace lshl::demux {

auto init_logging() -> void {
  namespace logging = boost::log;
  // NOLINTNEXTLINE(misc-include-cleaner)
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
}

}  // namespace lshl::demux

#endif  // DEMUX_UTIL_H