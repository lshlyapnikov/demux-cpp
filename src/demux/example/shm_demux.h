// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "../core/demultiplexer.h"
#include "../util/xxhash_util.h"
#include "./market_data.h"

namespace lshl::demux::example {

using std::size_t;
using std::uint16_t;

auto init_logging() noexcept -> void;

auto main_(std::span<char*> args) noexcept(false) -> int;

template <size_t L, uint16_t M>
auto start_writer(uint8_t total_reader_num, uint64_t msg_num, bool zero_copy) noexcept(false) -> void;

template <size_t L, uint16_t M>
auto run_writer_loop(lshl::demux::core::DemuxWriter<L, M, false>& writer, uint64_t msg_num) noexcept(false) -> void;

template <size_t L, uint16_t M>
auto run_writer_loop_zero_copy(lshl::demux::core::DemuxWriter<L, M, false>& writer, uint64_t msg_num) noexcept(false)
    -> void;

template <class T, size_t L, uint16_t M>
[[nodiscard]] inline auto write_(lshl::demux::core::DemuxWriter<L, M, false>* writer, const T& md) noexcept -> bool;

template <size_t L, uint16_t M>
[[nodiscard]] inline auto write_zero_copy_(
    lshl::demux::core::DemuxWriter<L, M, false>* writer,
    MarketDataUpdateGenerator* md_gen,
    lshl::demux::util::XXH64_util* hash
) noexcept(false) -> bool;

template <size_t L, uint16_t M>
auto start_reader(uint8_t reader_num, uint64_t msg_num) noexcept(false) -> void;

template <size_t L, uint16_t M>
auto run_reader_loop(lshl::demux::core::DemuxReader<L, M>& reader, uint64_t msg_num) noexcept(false) -> void;

auto inline calculate_latency(uint64_t x0) -> int64_t;

}  // namespace lshl::demux::example
