// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#ifndef LSHL_DEMUX_EXAMPLE_DEMUX_H
#define LSHL_DEMUX_EXAMPLE_DEMUX_H

#include <cstddef>
#include <cstdint>
#include <span>
#include "../core/demultiplexer.h"

namespace lshl::demux::example {

using std::size_t;
using std::uint16_t;

auto init_logging() noexcept -> void;

auto main_(std::span<char*> args) noexcept(false) -> int;

template <size_t L, uint16_t M>
auto start_writer(uint8_t total_reader_num, uint64_t msg_num) noexcept(false) -> void;

template <size_t L, uint16_t M>
auto run_writer_loop(lshl::demux::core::DemuxWriter<L, M, false>& pub, uint64_t msg_num) noexcept(false) -> void;

template <class T, size_t L, uint16_t M>
[[nodiscard]] inline auto send_(lshl::demux::core::DemuxWriter<L, M, false>& pub, const T& md) noexcept -> bool;

template <size_t L, uint16_t M>
auto start_reader(uint8_t reader_num, uint64_t msg_num) noexcept(false) -> void;

template <size_t L, uint16_t M>
auto run_reader_loop(lshl::demux::core::DemuxReader<L, M>& sub, uint64_t msg_num) noexcept(false) -> void;

auto calculate_latency(uint64_t x0) -> int64_t;

}  // namespace lshl::demux::example

#endif  // LSHL_DEMUX_EXAMPLE_DEMUX_H