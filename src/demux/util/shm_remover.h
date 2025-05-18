// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <boost/interprocess/shared_memory_object.hpp>
#include "./boost_log_util.h"

namespace lshl::demux::util {

struct ShmRemover {
  explicit ShmRemover(const char* name) : name_(name) {
    namespace bipc = boost::interprocess;
    const bool ok = bipc::shared_memory_object::remove(this->name_);
    if (ok) {
      LOG_WARNING << "[startup] removed shared_memory_object: " << this->name_;
    } else {
      LOG_INFO << "[startup] could not remove shared_memory_object: " << this->name_ << ", possible it did not exist.";
    }
  }

  ~ShmRemover() {
    namespace bipc = boost::interprocess;
    const bool ok = bipc::shared_memory_object::remove(this->name_);
    if (ok) {
      LOG_INFO << "[shutdown] removed shared_memory_object: " << this->name_;
    } else {
      LOG_WARNING << "[shutdown] could not remove shared_memory_object: " << this->name_;
    }
  }

  ShmRemover(const ShmRemover&) = delete;                         // copy constructor
  auto operator=(const ShmRemover&) -> ShmRemover& = delete;      // copy assignment
  ShmRemover(ShmRemover&&) noexcept = delete;                     // move constructor
  auto operator=(ShmRemover&&) noexcept -> ShmRemover& = delete;  // move assignment

 private:
  const char* name_;
};

}  // namespace lshl::demux::util
