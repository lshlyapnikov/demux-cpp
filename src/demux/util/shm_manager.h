// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <atomic>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstddef>
#include <cstdint>
#include <format>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
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
  std::array<M, N> value{};
};

template <size_t L>
struct alignas(CACHE_LINE_SIZE) ShmWriterData {
  CacheLinePaddedAtomic<uint64_t> downstream_sequence;
  CacheLinePaddedArray<uint8_t, L> buffer;
};

template <size_t R>
struct alignas(CACHE_LINE_SIZE) ShmReaderData {
  CacheLinePaddedArray<CacheLinePaddedAtomic<uint64_t>, R> upstream_sequences{};
  CacheLinePaddedAtomic<size_t> active_reader_count{};
};

template <size_t R>
auto to_upstream_sequence_pointers(CacheLinePaddedArray<CacheLinePaddedAtomic<uint64_t>, R>& array) noexcept
    -> std::vector<atomic<uint64_t>*> {
  std::vector<std::atomic<uint64_t>*> result;
  result.reserve(R);
  std::array<CacheLinePaddedAtomic<uint64_t>, R>& tmp_array = array.value;

  for (size_t i = 0; i < R; ++i) {
    CacheLinePaddedAtomic<uint64_t>& x = tmp_array[i];
    result.push_back(&x.value);
  }

  return result;
}

template <size_t L, size_t R>
constexpr auto calculate_shared_mem_size() noexcept -> std::size_t {
  const std::size_t min_size = sizeof(ShmWriterData<L>) + sizeof(ShmReaderData<R>) + BOOST_IPC_INTERNAL_METADATA_SIZE;
  const std::size_t quotient = min_size / LINUX_PAGE_SIZE;
  const std::size_t reminder = min_size % LINUX_PAGE_SIZE;
  if (reminder > 0) {
    return (quotient + 1) * LINUX_PAGE_SIZE;
  } else {
    return quotient * LINUX_PAGE_SIZE;
  }
}

template <size_t L, size_t R>
class ShmManager {
 private:
  std::optional<ShmRemover> remover_;  // needed for create_only mode to remove shared memory on destruction
  const std::string name_;
  bipc::managed_shared_memory segment_;

 public:
  explicit ShmManager(bipc::create_only_t /*unused*/, const std::string& name, const size_t segment_size)
      : remover_(std::in_place, name.c_str()), name_(name), segment_(bipc::create_only, name.c_str(), segment_size) {
    log_startup_state("created");
  }

  explicit ShmManager(bipc::open_only_t /*unused*/, const std::string& name)
      : remover_(std::nullopt), name_(name), segment_(bipc::open_only, name.c_str()) {
    log_startup_state("opened");
  }

  ~ShmManager() = default;

  ShmManager(const ShmManager&) = delete;                         // copy constructor
  auto operator=(const ShmManager&) -> ShmManager& = delete;      // copy assignment
  ShmManager(ShmManager&&) noexcept = delete;                     // move constructor
  auto operator=(ShmManager&&) noexcept -> ShmManager& = delete;  // move assignment

  [[nodiscard]] auto construct_shm_writer_data() noexcept(false) -> ShmWriterData<L>* {
    return construct_object<ShmWriterData<L>>(SHM_WRITER_DATA);
  }

  [[nodiscard]] auto find_shm_writer_data() noexcept(false) -> ShmWriterData<L>* {
    return find_object<ShmWriterData<L>>(SHM_WRITER_DATA);
  }

  [[nodiscard]] auto construct_shm_reader_data() noexcept(false) -> ShmReaderData<R>* {
    return construct_object<ShmReaderData<R>>(SHM_READER_DATA);
  }

  [[nodiscard]] auto find_shm_reader_data() noexcept(false) -> ShmReaderData<R>* {
    return find_object<ShmReaderData<R>>(SHM_READER_DATA);
  }

  [[nodiscard]] auto get_free_memory() const noexcept -> size_t { return this->segment_.get_free_memory(); }

 private:
  static constexpr std::string SHM_WRITER_DATA = "shm_writer";
  static constexpr std::string SHM_READER_DATA = "shm_reader";

  auto log_startup_state(const std::string& context) -> auto {
    LOG_INFO << "[startup] " << context << " managed_shared_memory segment: " << this->name_
             << ", buffer size (L): " << L << ", max readers num (R): " << R
             << ", segment.size: " << this->segment_.get_size()
             << ", segment.free_memory: " << this->segment_.get_free_memory();
  }

  template <typename T>
  [[nodiscard]] auto construct_object(const std::string& name) noexcept(false) -> T* {
    T* result = segment_.construct<T>(name.c_str())();
    if (result == nullptr) {
      throw std::runtime_error(std::format("cannot construct object in shared memory: {}", name));
    }
    LOG_INFO << "object allocated in shared memory: " << name
             << ", cache line aligned: " << is_cache_line_aligned(result)
             << ", segment.free_memory : " << this->segment_.get_free_memory();
    return result;
  }

  template <typename T>
  [[nodiscard]] auto find_object(const char* name) noexcept(false) -> T* {
    T* result = segment_.template find<T>(name).first;
    if (result == nullptr) {
      throw std::runtime_error(std::format("cannot find object in shared memory: {}", name));
    }
    LOG_INFO << "object found in shared memory: " << name << ", cache line aligned: " << is_cache_line_aligned(result);
    return result;
  }
};

}  // namespace lshl::demux::util