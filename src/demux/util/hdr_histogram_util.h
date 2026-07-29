// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <hdr/hdr_histogram.h>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace lshl::demux::util {

using std::int64_t;

struct HDR_histogram_util {
  static constexpr int64_t LOWEST_DISCERNIBLE_VALUE = 1L;              // Minimum value that can be tracked
  static constexpr int64_t HIGHEST_TRACKABLE_VALUE = 10'000'000'000L;  // Maximum value to be tracked
  static constexpr int SIGNIFICANT_FIGURES = 1;                        // Number of significant figures to maintain

  explicit HDR_histogram_util(
      int64_t lowest_discernible_value = LOWEST_DISCERNIBLE_VALUE,
      int64_t highest_trackable_value = HIGHEST_TRACKABLE_VALUE,
      int significant_figures = SIGNIFICANT_FIGURES
  ) noexcept(false) {
    if (hdr_init(lowest_discernible_value, highest_trackable_value, significant_figures, &histogram_) != 0) {
      throw std::domain_error("hdr_init failed");
    }
  };

  ~HDR_histogram_util() { hdr_close(this->histogram_); }

  HDR_histogram_util(const HDR_histogram_util&) = delete;                         // copy constructor
  auto operator=(const HDR_histogram_util&) -> HDR_histogram_util& = delete;      // copy assignment
  HDR_histogram_util(HDR_histogram_util&&) noexcept = delete;                     // move constructor
  auto operator=(HDR_histogram_util&&) noexcept -> HDR_histogram_util& = delete;  // move assignment

  [[nodiscard]] auto record_value(std::int64_t value) noexcept -> bool {
    return hdr_record_value(this->histogram_, value);
  }

  auto print_report() const noexcept { hdr_percentiles_print(this->histogram_, stdout, 2, 1.0, format_type::CLASSIC); }

 private:
  hdr_histogram* histogram_{nullptr};
};

}  // namespace lshl::demux::util
