// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner)

#define UNIT_TEST

#include "../core/endpoint_id.h"
#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <boost/lexical_cast.hpp>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include "./endpoint_id_gen.h"

using lshl::demux::core::EndpointId;

TEST(EndpointIdTest, Constants) {
  EXPECT_EQ(lshl::demux::core::MAX_ENDPOINT_NUM, 64);
}

TEST(EndpointIdTest, DefaultConstructor) {
  const EndpointId x;
  const EndpointId y{1};
  EXPECT_EQ(x.value(), y.value());
  ASSERT_TRUE(EndpointId{2} < EndpointId{3});
}

TEST(EndpointIdTest, OperatorEquals) {
  rc::check([](const EndpointId& a, const EndpointId& b) {
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

TEST(EndpointIdTest, OperatorLessThan) {
  rc::check([](const EndpointId& a, const EndpointId& b) {
    ASSERT_FALSE(a < a);
    ASSERT_FALSE(b < b);
    if (a.value() < b.value()) {
      ASSERT_TRUE(a < b);
    } else {
      ASSERT_FALSE(a < b);
    }
  });
}

TEST(EndpointIdTest, EndpointIdManualCheck) {
  for (const uint8_t num :
       {static_cast<uint8_t>(0), static_cast<uint8_t>(65), static_cast<uint8_t>(128), static_cast<uint8_t>(255)}) {
    ASSERT_THROW({ std::ignore = EndpointId(num); }, std::invalid_argument);
    ASSERT_THROW({ std::ignore = EndpointId::all_endpoints_mask(num); }, std::invalid_argument);
  }
  {
    const EndpointId reader_id(1);
    ASSERT_EQ(0b1, reader_id.mask());

    const uint64_t mask = EndpointId::all_endpoints_mask(1);
    ASSERT_EQ(0b1, mask);
  }
  {
    const EndpointId reader_id(2);
    ASSERT_EQ(0b10, reader_id.mask());

    const uint64_t mask = EndpointId::all_endpoints_mask(2);
    ASSERT_EQ(0b11, mask);
  }
  {
    const EndpointId reader_id(3);
    ASSERT_EQ(0b100, reader_id.mask());

    const uint64_t mask = EndpointId::all_endpoints_mask(3);
    ASSERT_EQ(0b111, mask);
  }
  {
    const EndpointId reader_id(4);
    ASSERT_EQ(0b1000, reader_id.mask());

    const uint64_t mask = EndpointId::all_endpoints_mask(4);
    ASSERT_EQ(0b1111, mask);
  }
  {
    const EndpointId reader_id(31);
    ASSERT_EQ(0b1000000000000000000000000000000, reader_id.mask());

    const uint64_t mask = EndpointId::all_endpoints_mask(31);
    ASSERT_EQ(0b1111111111111111111111111111111, mask);
  }
  {
    const EndpointId reader_id(32);
    ASSERT_EQ(0b10000000000000000000000000000000, reader_id.mask());

    const uint64_t mask = EndpointId::all_endpoints_mask(32);
    ASSERT_EQ(0b11111111111111111111111111111111, mask);
  }
  {
    const EndpointId reader_id(64);
    ASSERT_EQ(0b1000000000000000000000000000000000000000000000000000000000000000, reader_id.mask());

    const uint64_t mask = EndpointId::all_endpoints_mask(64);
    ASSERT_EQ(0b1111111111111111111111111111111111111111111111111111111111111111, mask);
  }
}

TEST(EndpointIdTest, EndpointId) {
  rc::check([](const uint8_t num) {
    if (num > lshl::demux::core::MAX_ENDPOINT_NUM || num == 0) {
      ASSERT_THROW({ std::ignore = EndpointId{num}; }, std::invalid_argument);
    } else {
      const EndpointId reader_id{num};
      ASSERT_EQ(pow(2, (num - 1)), reader_id.mask());
      ASSERT_EQ(num, reader_id.value());
      std::ostringstream oss;
      oss << "EndpointId{value: " << static_cast<uint64_t>(reader_id.value()) << ", mask: " << reader_id.mask() << "}";
      const std::string expected = oss.str();
      const auto actual = boost::lexical_cast<std::string>(reader_id);
      ASSERT_EQ(expected, actual);
    }
  });
}

TEST(EndpointIdTest, EndpointIdVectorStreamOperator) {
  const std::vector readers{EndpointId(1), EndpointId(2), EndpointId(32)};
  const auto actual = boost::lexical_cast<std::string>(readers);
  ASSERT_EQ(
      "[EndpointId{value: 1, mask: 1}, EndpointId{value: 2, mask: 2}, EndpointId{value: 32, mask: 2147483648}]", actual
  );
}

TEST(EndpointIdTest, AllReadersMask) {
  rc::check([](const uint8_t num) {
    if (num > lshl::demux::core::MAX_ENDPOINT_NUM || num == 0) {
      ASSERT_THROW({ std::ignore = EndpointId::all_endpoints_mask(num); }, std::invalid_argument);
    } else {
      const uint64_t all_mask = EndpointId::all_endpoints_mask(num);
      ASSERT_EQ(pow(2, num) - 1, all_mask);
    }
  });
}

TEST(EndpointIdTest, EndpointIdGenerator) {
  rc::check([](const EndpointId& sub) { RC_TAG(sub.mask()); });
}

// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)
