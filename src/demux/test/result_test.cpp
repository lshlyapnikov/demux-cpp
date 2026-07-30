// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

#define UNIT_TEST

#include "../util/result.h"
#include <gtest/gtest.h>
#include <string>
#include <type_traits>

namespace {

using lshl::demux::util::empty_value;
using lshl::demux::util::EmptyResult;
using lshl::demux::util::error;
using lshl::demux::util::Result;
using lshl::demux::util::Unit;
using lshl::demux::util::value;

TEST(ResultTest, DefaultTemplateArgumentsAndConstructor) {
  // Result<> should default to Result<std::string, Unit>.
  // As a variant, it will default-construct to the first alternative E, which is std::string.
  const Result<> res;
  EXPECT_TRUE(res.is_error());
  EXPECT_FALSE(res.is_value());
  EXPECT_EQ(res.error(), "");
}

TEST(ResultTest, ErrorState) {
  // Test constructing a Result with the error type E (index 0).
  const Result<int, double> res{42};
  EXPECT_TRUE(res.is_error());
  EXPECT_FALSE(res.is_value());
  EXPECT_EQ(res.error(), 42);

  // Test with std::string error.
  const Result<std::string, int> res_str{std::string("failed")};
  EXPECT_TRUE(res_str.is_error());
  EXPECT_FALSE(res_str.is_value());
  EXPECT_EQ(res_str.error(), "failed");
}

TEST(ResultTest, SuccessState) {
  // Test constructing a Result with the success type R (index 1).
  const Result<int, double> res{3.14};
  EXPECT_TRUE(res.is_value());
  EXPECT_FALSE(res.is_error());
  EXPECT_DOUBLE_EQ(res.value(), 3.14);

  // Test with custom struct success.
  struct Dummy {
    int val;
  };
  const Result<std::string, Dummy> res_dummy{Dummy{100}};
  EXPECT_TRUE(res_dummy.is_value());
  EXPECT_FALSE(res_dummy.is_error());
  EXPECT_EQ(res_dummy.value().val, 100);
}

TEST(ResultTest, SuccessHelper) {
  // Test success helper function where E defaults to std::string.
  const auto res = value(123);
  static_assert(std::is_same_v<std::decay_t<decltype(res)>, Result<std::string, int>>);
  EXPECT_TRUE(res.is_value());
  EXPECT_FALSE(res.is_error());
  EXPECT_EQ(res.value(), 123);

  // Test success helper with explicit template arguments.
  const auto res_explicit = value<int, double>(3.14);
  static_assert(std::is_same_v<std::decay_t<decltype(res_explicit)>, Result<int, double>>);
  EXPECT_TRUE(res_explicit.is_value());
  EXPECT_FALSE(res_explicit.is_error());
  EXPECT_DOUBLE_EQ(res_explicit.value(), 3.14);
}

TEST(ResultTest, ErrorHelper) {
  // Test error helper function where E and R default.
  const auto res = error("some error");
  static_assert(std::is_same_v<std::decay_t<decltype(res)>, Result<std::string, Unit>>);
  EXPECT_TRUE(res.is_error());
  EXPECT_FALSE(res.is_value());
  EXPECT_EQ(res.error(), "some error");

  // Test error helper with explicit success template argument R.
  const auto res_explicit = error<std::string, double>("another error");
  static_assert(std::is_same_v<std::decay_t<decltype(res_explicit)>, Result<std::string, double>>);
  EXPECT_TRUE(res_explicit.is_error());
  EXPECT_FALSE(res_explicit.is_value());
  EXPECT_EQ(res_explicit.error(), "another error");
}

TEST(ResultTest, EmptyResultAndSuccessV) {
  // EmptyResult is Result<std::string, Unit>
  const EmptyResult res_success = empty_value;
  EXPECT_TRUE(res_success.is_value());
  EXPECT_FALSE(res_success.is_error());
  EXPECT_EQ(res_success.value(), Unit{});

  const EmptyResult res_error = error("error occurred");
  EXPECT_TRUE(res_error.is_error());
  EXPECT_FALSE(res_error.is_value());
  EXPECT_EQ(res_error.error(), "error occurred");
}

}  // namespace

// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner, readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
