// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include "../util/boost_log_util.h"
#include "../util/operators.h"
#include "../util/result.h"
#include "./demux_writer.h"

namespace lshl::demux::core {

using std::atomic;
using std::size_t;
using std::span;
using std::uint64_t;
using std::uint8_t;

template <typename A>
concept WriterStateLike = requires(const A& a) {
  { a.warning_attempt_threshold() } -> std::same_as<std::size_t>;
  //   { a.last_error() } -> std::same_as<const std::optional<std::string>&>;
};

template <typename Fn, typename Context, typename Msg, typename Ret = void>
concept MessageSupplierLike =
    std::invocable<Fn, Context*, Msg*> && std::same_as<std::invoke_result_t<Fn, Context*, Msg*>, Ret>;

template <
    size_t L,
    uint16_t M,
    WriterStateLike State,
    typename Msg,
    MessageSupplierLike<State, Msg, util::Result<std::string, bool>> MessageSupplierFn>
auto run_writer_loop(DemuxWriter<L, M, false>* writer, State* state, MessageSupplierFn supply_msg) noexcept
    -> util::EmptyResult {
  LOG_INFO << "[run_writer_loop] start: " << *writer;

  Msg msg{};

  while (true) {
    const util::Result<std::string, bool> supply_result = supply_msg(state, &msg);
    if (supply_result.is_error()) [[unlikely]] {
      return util::error(supply_result.error());
    }
    const bool last_msg = !supply_result.value();

    util::EmptyResult write_result = write_msg<L, M, State, Msg>(writer, state, msg);
    if (write_result.is_error()) [[unlikely]] {
      LOG_ERROR << "[run_writer_loop] exit(error): write failed, dropped last message, error=" << supply_result.error()
                << ", writer=" << writer;
      return write_result;
    }

    if (last_msg) [[unlikely]] {
      LOG_INFO << "[run_writer_loop] exit(ok): " << *writer;
      return util::empty_value;
    }
  }
}

template <size_t L, uint16_t M, WriterStateLike State, typename Msg>
[[nodiscard]] inline auto write_msg(DemuxWriter<L, M, false>* writer, const State* state, const Msg& msg) noexcept
    -> util::EmptyResult {
  size_t attempt = 0;

  while (true) {
    const WriteResult result = writer->template write_safe<Msg>(msg);
    switch (result) {
      case WriteResult::Success:
        return util::empty_value;
      case WriteResult::Error:
        return util::error("unrecoverable writer error, check the log");
      case WriteResult::Repeat:
        attempt += 1;
        if (attempt % state->warning_attempt_threshold() == 0) {
          LOG_WARNING << "one or more readers are lagging, wraparound is blocked(?), write attempt: " << attempt
                      << ", writer: " << writer;
        }
        continue;
    }
  }
}

template <class T, size_t L, uint16_t M, WriterStateLike State>
[[nodiscard]] inline auto
write_span(DemuxWriter<L, M, false>* writer, const State* state, const span<uint8_t>& msg) noexcept
    -> util::EmptyResult {
  static vector<uint64_t> upstream_sequences;
  int attempt = 0;

  while (true) {
    const WriteResult result = writer->write(msg);
    switch (result) {
      case WriteResult::Success:
        return util::empty_value;
      case WriteResult::Error:
        return util::error("unrecoverable writer error, check the log");
      case WriteResult::Repeat:
        attempt += 1;
        if (attempt % state->warning_attempt_threshold == 0) {
          writer->upstream_sequences(&upstream_sequences);
          LOG_WARNING << "one or more readers are lagging, wraparound is blocked, write attempt: " << attempt
                      << ", writer sequence: " << writer->message_count()
                      << ", downstream sequence: " << writer->downstream_sequence()
                      << ", upstream sequences: " << util::log_vector{upstream_sequences};
        }
        continue;
    }
  }
}
}  // namespace lshl::demux::core