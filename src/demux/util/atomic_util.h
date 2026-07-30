// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <concepts>
#include <thread>
#include "../util/boost_log_util.h"

namespace lshl::demux::util {

template <typename T, std::memory_order MemOrder = std::memory_order_seq_cst>
  requires std::integral<T> && std::atomic<T>::is_always_lock_free
auto wait_for_count(const std::atomic<T>* counter, const T target) noexcept -> void {
  static_assert(
      MemOrder == std::memory_order_relaxed || MemOrder == std::memory_order_consume ||
          MemOrder == std::memory_order_acquire || MemOrder == std::memory_order_seq_cst,
      "Invalid memory order for atomic load/wait operations"
  );
  assert(counter != nullptr);
  T current = counter->load(MemOrder);
  while (current < target) {
    LOG_INFO << "current=" << current << ", target=" << target;
    counter->wait(current, MemOrder);
    current = counter->load(MemOrder);
  }
}

template <typename T, std::memory_order MemOrder = std::memory_order_seq_cst>
  requires std::integral<T> && std::atomic<T>::is_always_lock_free
auto increment_count(std::atomic<T>* counter) noexcept -> T {
  static_assert(
      MemOrder == std::memory_order_release || MemOrder == std::memory_order_acq_rel ||
          MemOrder == std::memory_order_seq_cst,
      "Invalid memory order for atomic increment/notify operations"
  );
  assert(counter != nullptr);
  T new_value = counter->fetch_add(1, MemOrder) + 1;
  counter->notify_all();
  return new_value;
}

template <typename T, std::memory_order MemOrder = std::memory_order_seq_cst>
  requires std::integral<T> && std::atomic<T>::is_always_lock_free
auto wait_for_count_ipc(const std::atomic<T>* counter, const T target) noexcept -> void {
  static_assert(
      MemOrder == std::memory_order_relaxed || MemOrder == std::memory_order_consume ||
          MemOrder == std::memory_order_acquire || MemOrder == std::memory_order_seq_cst,
      "Invalid memory order for atomic load operations"
  );
  assert(counter != nullptr);
  const auto delta = std::chrono::seconds(1);

  size_t sleep_count = 0;
  T current = counter->load(MemOrder);
  while (current < target) {
    const auto sleep_duration = sleep_count * delta;
    LOG_INFO << "current=" << current << ", target=" << target << ", sleep_duration=" << sleep_duration;
    std::this_thread::sleep_for(sleep_duration);
    sleep_count += 1;
    current = counter->load(MemOrder);
  }
}

template <typename T, std::memory_order MemOrder = std::memory_order_seq_cst>
  requires std::integral<T> && std::atomic<T>::is_always_lock_free
auto increment_count_ipc(std::atomic<T>* counter) noexcept -> T {
  static_assert(
      MemOrder == std::memory_order_release || MemOrder == std::memory_order_acq_rel ||
          MemOrder == std::memory_order_seq_cst,
      "Invalid memory order for atomic increment operations"
  );
  assert(counter != nullptr);
  T new_value = counter->fetch_add(1, MemOrder) + 1;
  return new_value;
}

}  // namespace lshl::demux::util