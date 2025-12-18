// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <atomic>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include "./boost_log_util.h"
#include "./shm_remover.h"

namespace lshl::demux::util {

namespace bipc = boost::interprocess;

using std::atomic;
using std::size_t;

// Linux memory page size
constexpr std::size_t LINUX_PAGE_SIZE = 4096;

constexpr std::size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;

template <typename T>
auto is_cache_line_aligned(const T* ptr) noexcept -> bool {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return (reinterpret_cast<std::uintptr_t>(ptr) % CACHE_LINE_SIZE) == 0;
}

struct alignas(CACHE_LINE_SIZE) CacheLinePaddedAtomicUint64 {
  std::atomic<std::uint64_t> value{0};
  std::array<std::uint8_t, CACHE_LINE_SIZE - sizeof(std::atomic<std::uint64_t>)> padding{};
};

template <std::size_t READERS_NUM, std::size_t BUFFER_SIZE>
struct alignas(CACHE_LINE_SIZE) ShmPrimitives {
  CacheLinePaddedAtomicUint64 writer_sequence;
  std::array<CacheLinePaddedAtomicUint64, READERS_NUM> reader_sequences;
  std::array<std::uint8_t, BUFFER_SIZE> buffer{};
};

template <size_t READERS_NUM, size_t BUFFER_SIZE>
class ShmManager {
 private:
  std::optional<ShmRemover> remover_;
  const std::string name_;
  bipc::managed_shared_memory segment_;

 public:
  explicit ShmManager(bipc::create_only_t /*unused*/, const std::string& name, const std::size_t segment_size)
      : remover_(std::in_place, name.c_str()), name_(name), segment_(bipc::create_only, name.c_str(), segment_size) {
    LOG_INFO << "[startup] created managed_shared_memory segment: " << this->name_ << ", READERS_NUM: " << READERS_NUM
             << ", BUFFER_SIZE: " << BUFFER_SIZE
             << ", primitive size: " << sizeof(ShmPrimitives<READERS_NUM, BUFFER_SIZE>)
             << ", requested segment_size: " << segment_size << ", segment.size: " << this->segment_.get_size()
             << ", segment.free_memory: " << this->segment_.get_free_memory();
  }

  explicit ShmManager(bipc::open_only_t /*unused*/, const std::string& name)
      : remover_(std::nullopt), name_(name), segment_(bipc::open_only, name.c_str()) {
    LOG_INFO << "[startup] opened managed_shared_memory segment: " << this->name_ << ", BUFFER_SIZE: " << BUFFER_SIZE
             << ", primitive size: " << sizeof(ShmPrimitives<READERS_NUM, BUFFER_SIZE>)
             << ", segment.size: " << this->segment_.get_size()
             << ", segment.free_memory: " << this->segment_.get_free_memory();
  }

  ~ShmManager() = default;

  ShmManager(const ShmManager&) = delete;                         // copy constructor
  auto operator=(const ShmManager&) -> ShmManager& = delete;      // copy assignment
  ShmManager(ShmManager&&) noexcept = delete;                     // move constructor
  auto operator=(ShmManager&&) noexcept -> ShmManager& = delete;  // move assignment

  [[nodiscard]] auto construct_primitives() noexcept(false) -> ShmPrimitives<READERS_NUM, BUFFER_SIZE>* {
    auto* result = segment_.template construct<ShmPrimitives<READERS_NUM, BUFFER_SIZE>>("primitives")();
    LOG_INFO << "[construct_primitives] primitives allocated, segment.free_memory: " << this->segment_.get_free_memory()
             << " cache line aligned: " << is_cache_line_aligned(result);
    return result;
  }

  [[nodiscard]] auto find_primitives() noexcept -> ShmPrimitives<READERS_NUM, BUFFER_SIZE>* {
    auto* result = segment_.template find<ShmPrimitives<READERS_NUM, BUFFER_SIZE>>("primitives").first;
    LOG_INFO << "[find_primitives] primitives found, segment.free_memory: " << this->segment_.get_free_memory()
             << " cache line aligned: " << is_cache_line_aligned(result);
    return result;
  }
};
// NOLINTEND(misc-include-cleaner)

}  // namespace lshl::demux::util