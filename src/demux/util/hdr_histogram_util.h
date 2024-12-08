// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#ifndef LSHL_UTIL_HDR_HISTOGRAM_UTIL_H
#define LSHL_UTIL_HDR_HISTOGRAM_UTIL_H

#include <hdr/hdr_histogram.h>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace lshl::demux::util {

struct HDR_histogram_util {
  explicit HDR_histogram_util() noexcept(false) {
    using std::int64_t;
    // initialize hdr_histogram
    const int64_t lowest_discernible_value = 1L;          // Minimum value that can be tracked
    const int64_t highest_trackable_value = 3600000000L;  // Maximum value to be tracked (e.g., 1 hour in microseconds)
    const int significant_figures = 1;                    // Number of significant figures to maintain

    if (hdr_init(lowest_discernible_value, highest_trackable_value, significant_figures, &histogram_) != 0) {
      throw std::domain_error("hdr_init failed");
    }
  };

  ~HDR_histogram_util() { hdr_close(this->histogram_); }

  HDR_histogram_util(const HDR_histogram_util&) = delete;                         // copy constructor
  auto operator=(const HDR_histogram_util&) -> HDR_histogram_util& = delete;      // copy assignment
  HDR_histogram_util(HDR_histogram_util&&) noexcept = delete;                     // move constructor
  auto operator=(HDR_histogram_util&&) noexcept -> HDR_histogram_util& = delete;  // move assignment

  auto record_value(std::int64_t value) noexcept(false) -> void {
    if (!hdr_record_value(this->histogram_, value)) {
      throw std::domain_error("hdr_record_value failed");
    }
  }

  auto print_report() { hdr_percentiles_print(this->histogram_, stdout, 2, 1.0, format_type::CLASSIC); }

 private:
  hdr_histogram* histogram_{nullptr};
};

}  // namespace lshl::demux::util

#endif  // LSHL_UTIL_HDR_HISTOGRAM_UTIL_H