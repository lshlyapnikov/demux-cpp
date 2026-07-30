// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <xxhash.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include "../core/demux_reader.h"
#include "../core/demux_writer.h"
#include "../util/hdr_histogram_util.h"
#include "../util/result.h"
#include "../util/xxhash_util.h"
#include "./market_data.h"

namespace lshl::demux::example {

using lshl::demux::core::DemuxReader;
using lshl::demux::core::DemuxWriter;
using lshl::demux::core::ReaderId;
using lshl::demux::util::XXH64_util;

using std::size_t;
using std::string;
using std::uint16_t;
using std::uint8_t;
using std::vector;

class BaseState {
 private:
  size_t warning_attempt_threshold_;
  size_t message_limit_;
  size_t message_counter_{0};
  XXH64_util hash_;

 public:
  BaseState(size_t warning_attempt_threshold, size_t message_limit)
      : warning_attempt_threshold_(warning_attempt_threshold), message_limit_(message_limit) {};

  [[nodiscard]] auto warning_attempt_threshold() const noexcept -> size_t { return this->warning_attempt_threshold_; };
  [[nodiscard]] auto message_limit() const noexcept -> size_t { return this->message_limit_; };
  [[nodiscard]] auto message_counter() const noexcept -> size_t { return this->message_counter_; };
  auto increment_message_counter() noexcept -> size_t { return ++this->message_counter_; }
  auto update_hash(const void* input, size_t size) noexcept(false) -> void { this->hash_.update(input, size); }
  [[nodiscard]] auto hash_digest() const noexcept -> XXH64_hash_t { return this->hash_.digest(); }
};

class WriterState : public BaseState {
 public:
  WriterState(size_t warning_attempt_threshold, size_t message_limit)
      : BaseState(warning_attempt_threshold, message_limit) {};
};

class ReaderState : public BaseState {
 private:
  util::HDR_histogram_util histogram;

 public:
  ReaderState(size_t warning_attempt_threshold, size_t message_limit)
      : BaseState(warning_attempt_threshold, message_limit) {};

  [[nodiscard]] auto record_latency(std::int64_t value) noexcept -> bool { return this->histogram.record_value(value); }
  auto print_latency_report() const noexcept { this->histogram.print_report(); }
};

auto main_(std::span<char*> args) noexcept(false) -> int;

auto parse_reader_ids(const string& comma_separated_list) -> std::vector<ReaderId>;

auto parse_reader_id(const string& str_id) -> ReaderId;

template <size_t L, uint16_t M, size_t R>
auto start_writer(
    const string& shared_memory_name,
    const vector<ReaderId>& reader_ids,
    const uint64_t msg_num,
    bool zero_copy,
    bool calculate_hash
) noexcept(false) -> void;

auto supply_market_data(WriterState* context, MarketDataUpdate* md) -> util::Result<std::string, bool>;

auto supply_market_data_and_calc_hash(WriterState* context, MarketDataUpdate* md) -> util::Result<std::string, bool>;

auto consume_market_data(ReaderState* context, const MarketDataUpdate* md) -> util::Result<std::string, bool>;

auto consume_market_data_and_calc_hash(ReaderState* context, const MarketDataUpdate* md)
    -> util::Result<std::string, bool>;

template <class T, size_t L, uint16_t M>
[[nodiscard]] inline auto write(DemuxWriter<L, M, false>* writer, const T& md) noexcept -> bool;

template <size_t L, uint16_t M>
auto run_writer_loop_zero_copy(DemuxWriter<L, M, false>* writer, uint64_t msg_num) noexcept(false) -> void;

template <size_t L, uint16_t M>
[[nodiscard]] inline auto
write_zero_copy(DemuxWriter<L, M, false>* writer, lshl::demux::util::XXH64_util* hash) noexcept(false) -> bool;

template <size_t L, uint16_t M, size_t R>
auto start_reader(
    const string& shared_memory_name,
    const ReaderId& reader_id,
    uint64_t msg_num,
    bool calculate_hash
) noexcept(false) -> void;

auto inline calculate_latency(uint64_t x0) -> int64_t;

}  // namespace lshl::demux::example
