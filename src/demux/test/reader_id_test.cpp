// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner)

#define UNIT_TEST

#include "../core/reader_id.h"
#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <boost/lexical_cast.hpp>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include "./reader_id_gen.h"

using lshl::demux::core::ReaderId;

TEST(ReaderIdTest, DefaultConstructor) {
  const ReaderId x{};
  const ReaderId y{1};
  EXPECT_EQ(x.value(), y.value());
  ASSERT_TRUE(ReaderId{2} < ReaderId{3});
}

TEST(ReaderIdTest, OperatorEquals) {
  rc::check([](const ReaderId& a, const ReaderId& b) {
    ASSERT_EQ(a, a);
    ASSERT_EQ(b, b);
    if (a.value() == b.value()) {
      ASSERT_EQ(a, b);
      ASSERT_EQ(b, a);
    } else {
      ASSERT_NE(a, b);
      ASSERT_NE(b, a);
    }
  });
}

TEST(ReaderIdTest, OperatorLessThan) {
  rc::check([](const ReaderId& a, const ReaderId& b) {
    ASSERT_FALSE(a < a);
    ASSERT_FALSE(b < b);
    if (a.value() < b.value()) {
      ASSERT_TRUE(a < b);
    } else {
      ASSERT_FALSE(a < b);
    }
  });
}

TEST(ReaderIdTest, ReaderId) {
  rc::check([](const uint8_t num) {
    const ReaderId reader_id{num};
    ASSERT_EQ(num, reader_id.value());
    std::ostringstream oss;
    oss << "ReaderId{value: " << static_cast<uint64_t>(reader_id.value()) << "}";
    const std::string expected = oss.str();
    const auto actual = boost::lexical_cast<std::string>(reader_id);
    ASSERT_EQ(expected, actual);
  });
}

TEST(ReaderIdTest, ReaderIdVectorStreamOperator) {
  const std::vector readers{ReaderId(1), ReaderId(2), ReaderId(32)};
  const auto actual = boost::lexical_cast<std::string>(readers);
  ASSERT_EQ("[ReaderId{value: 1}, ReaderId{value: 2}, ReaderId{value: 32}]", actual);
}

TEST(ReaderIdTest, ReaderIdGenerator) {
  rc::check([](const ReaderId& sub) { RC_TAG(sub.value()); });
}

// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)
