#pragma once

#include <boost/log/trivial.hpp>  // IWYU pragma: export

// clang-format off

// Always available logging macros (never disabled)
#define LOG_INFO BOOST_LOG_TRIVIAL(info)
#define LOG_WARNING BOOST_LOG_TRIVIAL(warning)
#define LOG_ERROR BOOST_LOG_TRIVIAL(error)
#define LOG_FATAL BOOST_LOG_TRIVIAL(fatal)

#ifndef NDEBUG
  // Debug-only logging
  #define LOG_DEBUG BOOST_LOG_TRIVIAL(debug)
  #define LOG_TRACE BOOST_LOG_TRIVIAL(trace)
#else
  // Compile out debug/trace logging completely
  #define LOG_DEBUG if (false) BOOST_LOG_TRIVIAL(debug)
  #define LOG_TRACE if (false) BOOST_LOG_TRIVIAL(trace)
#endif

// clang-format on