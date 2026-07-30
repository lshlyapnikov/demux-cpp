// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include "../util/boost_log_util.h"
#include "../util/result.h"
#include "./demux_writer.h"
#include "demux_reader.h"

namespace lshl::demux::core {

using std::atomic;
using std::size_t;
using std::span;
using std::uint64_t;
using std::uint8_t;

template <typename A>
concept ReaderStateLike = requires(const A& a) {
  { a.warning_attempt_threshold() } -> std::same_as<std::size_t>;
  //   { a.last_error() } -> std::same_as<const std::optional<std::string>&>;
};

template <typename Fn, typename State, typename Msg, typename Ret = void>
concept MessageConsumerLike =
    std::invocable<Fn, State*, const Msg*> && std::same_as<std::invoke_result_t<Fn, State*, const Msg*>, Ret>;

template <
    size_t L,
    uint16_t M,
    ReaderStateLike State,
    typename Msg,
    MessageConsumerLike<State, Msg, util::Result<std::string, bool>> MessageConsumerFn>
auto run_reader_loop_unsafe(DemuxReader<L, M>* reader, State* state, MessageConsumerFn consume_msg) noexcept
    -> util::EmptyResult {
  LOG_INFO << "started reader loop: " << *reader;

  while (true) {
    const std::optional<const Msg*> opt_msg = reader->template next_unsafe<Msg>();
    if (opt_msg.has_value()) {
      const Msg* msg = opt_msg.value();
      const util::Result<std::string, bool> result = consume_msg(state, msg);
      if (result.is_error()) {
        return util::error(result.error());
      } else if (!result.value()) {
        LOG_INFO << "finished reader loop gracefully: " << *reader;
        return util::empty_value;
      }
    }
  }
}

template <
    size_t L,
    uint16_t M,
    ReaderStateLike State,
    MessageConsumerLike<State, const span<uint8_t>, util::Result<std::string, bool>> MessageConsumerFn>
auto run_reader_loop(DemuxReader<L, M>* reader, State* state, MessageConsumerFn consume_msg) noexcept
    -> util::EmptyResult {
  LOG_INFO << "started reader loop: " << *reader;

  while (true) {
    const std::span<uint8_t> msg = reader->next();
    if (!msg.empty()) {
      const util::Result<std::string, bool> result = consume_msg(state, msg);
      if (result.is_error()) {
        return util::error(result.error());
      } else if (!result.value()) {
        LOG_INFO << "finished reader loop gracefully: " << *reader;
        return util::empty_value;
      }
    }
  }
}

}  // namespace lshl::demux::core