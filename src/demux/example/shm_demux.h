// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "../core/demux_reader.h"
#include "../core/demux_writer.h"
#include "./market_event.h"

namespace lshl::demux::example {

using lshl::demux::core::DemuxReader;
using lshl::demux::core::DemuxWriter;

using std::size_t;
using std::uint16_t;
using std::uint8_t;

auto init_logging() noexcept -> void;

auto main_(std::span<char*> args) noexcept(false) -> int;

auto wait_for_readers(const std::atomic<size_t>* startup_reader_counter, const uint8_t total_reader_num) -> void;

template <typename M, size_t N>
auto start_writer(uint8_t total_reader_num, uint64_t msg_num, bool emplace) noexcept(false) -> void;

template <typename M, size_t N, typename WriteFn>
auto run_writer_loop(DemuxWriter<M, N, false>* writer, uint64_t msg_num, WriteFn write_fn) noexcept(false) -> void;

template <typename M, size_t N>
[[nodiscard]] inline auto write(DemuxWriter<M, N, false>* writer, const M& md) noexcept -> bool;

template <typename M, size_t N>
[[nodiscard]] inline auto write_with_emplace(DemuxWriter<M, N, false>* writer, const M& md) noexcept -> bool;

template <typename M, size_t N>
auto start_reader(const core::ReaderId& reader_id, uint64_t msg_num) noexcept(false) -> void;

template <typename M, size_t N>
auto run_reader_loop(DemuxReader<M, N, false>* reader, uint64_t msg_num) noexcept(false) -> void;

auto inline calculate_latency(uint64_t x0) -> int64_t;

}  // namespace lshl::demux::example
