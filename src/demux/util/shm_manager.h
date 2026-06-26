// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <atomic>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include "./boost_log_util.h"
#include "./fast_math.h"
#include "./shm_remover.h"

namespace lshl::demux::util {

namespace bipc = boost::interprocess;

using std::atomic;
using std::size_t;
using std::uint64_t;
using std::uint8_t;

// names take up some space in the managed_shared_memory
constexpr std::size_t BOOST_IPC_INTERNAL_METADATA_SIZE = 512;

// Linux memory page size
constexpr size_t LINUX_PAGE_SIZE = 4096;

constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;

// TODO(Leonid): do you still need this?
constexpr auto calculate_required_shared_mem_size(
    const std::size_t data_size,
    const std::size_t metadata_size,
    const std::size_t page_size
) noexcept -> std::size_t {
  // names take some space in the managed_shared_memory, this is why BOOST_IPC_INTERNAL_METADATA_SIZE is added
  // total shared memory size, should be  a multiple of the page size (4kB on Linux). Because the operating system
  // performs mapping operations over whole pages. So, you don't waste memory.
  const std::size_t quotient = (data_size + metadata_size) / page_size;
  const std::size_t reminder = (data_size + metadata_size) % page_size;
  if (reminder > 0) {
    return (quotient + 1) * page_size;
  } else {
    return quotient * page_size;
  }
}

template <typename T>
auto is_cache_line_aligned(const T* ptr) noexcept -> bool {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return (reinterpret_cast<std::uintptr_t>(ptr) % CACHE_LINE_SIZE) == 0;
}

template <typename T>
struct alignas(CACHE_LINE_SIZE) CacheLinePaddedAtomic {
  std::atomic<T> value{0};
};

template <typename M, size_t N>
struct alignas(CACHE_LINE_SIZE) CacheLinePaddedArray {
  static_assert(is_power_of_2(N), "N must be a power of 2 for optimization");
  std::array<M, N> value{};
};

template <size_t L>
struct alignas(CACHE_LINE_SIZE) WriterShmData {
  CacheLinePaddedAtomic<size_t> downstream_sequence;
  CacheLinePaddedArray<uint8_t, L> buffer;
};

template <uint8_t R>
struct alignas(CACHE_LINE_SIZE) ReadersShmData {
  CacheLinePaddedArray<CacheLinePaddedAtomic<size_t>, R> upstream_sequences{};
};

template <typename M, size_t N, size_t R>
struct ShmData {
  static_assert(N % 2 == 0, "N must be a power of 2 for optimization");

  // SECTION 1: WRITER HOT (Reader Cold)
  alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail;

  // SECTION 2: THE DATA (Shared Hot)
  // Keep this aligned so it starts on a fresh cache line
  alignas(CACHE_LINE_SIZE) std::array<M, N> buffer;

  // TODO(Leonid): does this gap really improve anything???
  // SECTION 3: THE GAP (Critical)
  // Prevents the Prefetcher from crossing between Buffer and Reader state
  alignas(2 * CACHE_LINE_SIZE) std::array<uint8_t, 2 * CACHE_LINE_SIZE> unused_padding_{};

  // SECTION 4: READER HOT (Writer Cold)
  // Each Reader gets its own isolated cache line
  alignas(CACHE_LINE_SIZE) std::array<CacheLinePaddedAtomic<size_t>, R> heads{};
};

template <typename M, size_t N, size_t R>
class ShmManager {
 private:
  std::optional<ShmRemover> remover_;  // needed for create_only mode to remove shared memory on destruction
  const std::string name_;
  bipc::managed_shared_memory segment_;
  const uint8_t readers_num_;

 public:
  explicit ShmManager(
      bipc::create_only_t /*unused*/,
      const std::string& name,
      const size_t segment_size,
      const uint8_t readers_num
  )
      : remover_(std::in_place, name.c_str()),
        name_(name),
        segment_(bipc::create_only, name.c_str(), segment_size),
        readers_num_(readers_num) {
    LOG_INFO << "[startup] created managed_shared_memory segment: " << this->name_ << ", BUFFER_SIZE: " << N
             << ", readers_num: " << static_cast<int>(this->readers_num_) << ", R: " << R
             << ", requested segment_size: " << segment_size << ", segment.size: " << this->segment_.get_size()
             << ", segment.free_memory: " << this->segment_.get_free_memory();
  }

  explicit ShmManager(bipc::open_only_t /*unused*/, const std::string& name)
      : remover_(std::nullopt), name_(name), segment_(bipc::open_only, name.c_str()), readers_num_(0) {
    LOG_INFO << "[startup] opened managed_shared_memory segment: " << this->name_ << ", BUFFER_SIZE: " << N
             << ", readers_num: " << static_cast<int>(this->readers_num_)
             << ", segment.size: " << this->segment_.get_size()
             << ", segment.free_memory: " << this->segment_.get_free_memory();
  }

  ~ShmManager() = default;

  ShmManager(const ShmManager&) = delete;                         // copy constructor
  auto operator=(const ShmManager&) -> ShmManager& = delete;      // copy assignment
  ShmManager(ShmManager&&) noexcept = delete;                     // move constructor
  auto operator=(ShmManager&&) noexcept -> ShmManager& = delete;  // move assignment

  [[nodiscard]] auto construct_shm_data() noexcept(false) -> ShmData<M, N, R>* {
    auto* result = segment_.construct<ShmData<M, N, R>>("shm_data")();
    if (result == nullptr) {
      throw std::runtime_error("can't construct in shared memory: shm_data");
    }
    LOG_INFO << "[construct_shm_data] buffer allocated, segment.free_memory: " << this->segment_.get_free_memory()
             << " cache line aligned: " << is_cache_line_aligned(result);
    return result;
  }

  [[nodiscard]] auto find_shm_data() noexcept(false) -> ShmData<M, N, R>* {
    auto* result = segment_.template find<ShmData<M, N, R>>("shm_data").first;
    if (result == nullptr) {
      throw std::runtime_error("can't find in shared memory: shm_data");
    }
    LOG_INFO << "[find_shm_data] buffer found" << ", cache line aligned: " << is_cache_line_aligned(result);
    return result;
  }

  [[nodiscard]] auto construct_buffer() noexcept(false) -> std::array<M, N>* {
    auto* result = segment_.template construct<CacheLinePaddedArray<M, N>>("buffer")();
    if (result == nullptr) {
      throw std::runtime_error("can't construct in shared memory: buffer");
    }
    LOG_INFO << "[construct_buffer] buffer allocated, segment.free_memory: " << this->segment_.get_free_memory()
             << " cache line aligned: " << is_cache_line_aligned(result);
    return &result->value;
  }

  [[nodiscard]] auto construct_writer_tail() noexcept -> atomic<size_t>* {
    return construct_atomic<size_t>("writer_tail");
  }

  [[nodiscard]] auto construct_all_reader_heads() noexcept -> std::vector<const atomic<size_t>*> {
    std::vector<const atomic<size_t>*> result;
    result.reserve(this->readers_num_);
    for (uint8_t i = 0; i < this->readers_num_; ++i) {
      result.push_back(this->construct_reader_head(i));
    }
    return result;
  }

  [[nodiscard]] auto construct_reader_head(uint8_t reader_id) noexcept -> atomic<size_t>* {
    const std::string name = "reader_head_" + std::to_string(reader_id);
    return construct_atomic<size_t>(name);
  }

  [[nodiscard]] auto construct_startup_reader_counter() noexcept -> atomic<size_t>* {
    return construct_atomic<uint64_t>("startup_reader_counter");
  }

  [[nodiscard]] auto find_buffer() noexcept(false) -> std::array<M, N>* {
    auto* result = segment_.template find<CacheLinePaddedArray<M, N>>("buffer").first;
    if (result == nullptr) {
      throw std::runtime_error("can't find in shared memory: buffer");
    }
    LOG_INFO << "[find_buffer] buffer found" << ", cache line aligned: " << is_cache_line_aligned(result);
    return &result->value;
  }

  [[nodiscard]] auto find_writer_tail() noexcept -> atomic<size_t>* { return find_atomic<size_t>("writer_tail"); }

  [[nodiscard]] auto find_reader_head(uint8_t reader_id) noexcept -> atomic<size_t>* {
    const std::string name = "reader_head_" + std::to_string(reader_id);
    return find_atomic<size_t>(name);
  }

  [[nodiscard]] auto find_startup_reader_counter() noexcept -> atomic<size_t>* {
    return find_atomic<uint64_t>("startup_reader_counter");
  }

  [[nodiscard]] auto get_free_memory() const noexcept -> size_t { return this->segment_.get_free_memory(); }

 private:
  template <typename T>
  [[nodiscard]] auto construct_atomic(const std::string& name) noexcept(false) -> atomic<T>* {
    LOG_INFO << "[construct_atomic] " << name << " ...";
    auto* result = segment_.template construct<CacheLinePaddedAtomic<T>>(name.c_str())();
    if (result == nullptr) {
      throw std::runtime_error("can't construct in shared memory:" + name);
    }
    LOG_INFO << "[construct_atomic] " << name << " allocated, segment.free_memory: " << this->segment_.get_free_memory()
             << " cache line aligned: " << is_cache_line_aligned(result);
    return &result->value;
  }

  template <typename T>
  [[nodiscard]] auto find_atomic(const std::string& name) noexcept(false) -> atomic<T>* {
    LOG_INFO << "[find_atomic] " << name << " ...";
    auto* result = segment_.template find<CacheLinePaddedAtomic<T>>(name.c_str()).first;
    if (result == nullptr) {
      throw std::runtime_error("can't find in shared memory:" + name);
    }
    LOG_INFO << "[find_atomic] " << name << " success, cache line aligned: " << is_cache_line_aligned(result);
    return &result->value;
  }
};

}  // namespace lshl::demux::util