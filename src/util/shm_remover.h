// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#ifndef LSHL_UTIL_SHM_REMOVER_H
#define LSHL_UTIL_SHM_REMOVER_H

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/log/trivial.hpp>

namespace lshl::util {

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

}  // namespace lshl::util

#endif  // LSHL_UTIL_SHM_REMOVER_H