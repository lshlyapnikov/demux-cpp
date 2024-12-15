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
#include <stdexcept>
#include <string>
#include <tuple>
#include "./reader_id_gen.h"

using lshl::demux::core::ReaderId;

TEST(ReaderIdTest, Constants) {
  EXPECT_EQ(lshl::demux::core::MAX_READER_NUM, 64);
}

TEST(ReaderIdTest, ReaderIdManualCheck) {
  for (const uint8_t num :
       {static_cast<uint8_t>(0), static_cast<uint8_t>(65), static_cast<uint8_t>(128), static_cast<uint8_t>(255)}) {
    ASSERT_THROW({ std::ignore = ReaderId::create(num); }, std::invalid_argument);
    ASSERT_THROW({ std::ignore = ReaderId::all_readers_mask(num); }, std::invalid_argument);
  }
  {
    const ReaderId reader_id = ReaderId::create(1);
    ASSERT_EQ(0b1, reader_id.mask());

    const uint64_t mask = ReaderId::all_readers_mask(1);
    ASSERT_EQ(0b1, mask);
  }
  {
    const ReaderId reader_id = ReaderId::create(2);
    ASSERT_EQ(0b10, reader_id.mask());

    const uint64_t mask = ReaderId::all_readers_mask(2);
    ASSERT_EQ(0b11, mask);
  }
  {
    const ReaderId reader_id = ReaderId::create(3);
    ASSERT_EQ(0b100, reader_id.mask());

    const uint64_t mask = ReaderId::all_readers_mask(3);
    ASSERT_EQ(0b111, mask);
  }
  {
    const ReaderId reader_id = ReaderId::create(4);
    ASSERT_EQ(0b1000, reader_id.mask());

    const uint64_t mask = ReaderId::all_readers_mask(4);
    ASSERT_EQ(0b1111, mask);
  }
  {
    const ReaderId reader_id = ReaderId::create(31);
    ASSERT_EQ(0b1000000000000000000000000000000, reader_id.mask());

    const uint64_t mask = ReaderId::all_readers_mask(31);
    ASSERT_EQ(0b1111111111111111111111111111111, mask);
  }
  {
    const ReaderId reader_id = ReaderId::create(32);
    ASSERT_EQ(0b10000000000000000000000000000000, reader_id.mask());

    const uint64_t mask = ReaderId::all_readers_mask(32);
    ASSERT_EQ(0b11111111111111111111111111111111, mask);
  }
  {
    const ReaderId reader_id = ReaderId::create(64);
    ASSERT_EQ(0b1000000000000000000000000000000000000000000000000000000000000000, reader_id.mask());

    const uint64_t mask = ReaderId::all_readers_mask(64);
    ASSERT_EQ(0b1111111111111111111111111111111111111111111111111111111111111111, mask);
  }
}

TEST(ReaderIdTest, ReaderId) {
  rc::check([](const uint8_t num) {
    if (num > lshl::demux::core::MAX_READER_NUM || num == 0) {
      ASSERT_THROW({ std::ignore = ReaderId::create(num); }, std::invalid_argument);
    } else {
      const ReaderId reader_id = ReaderId::create(num);
      ASSERT_EQ(pow(2, (num - 1)), reader_id.mask());
      ASSERT_EQ(num, reader_id.number());
      std::ostringstream oss;
      oss << "ReaderId{number: " << static_cast<uint64_t>(reader_id.number()) << ", mask: " << reader_id.mask() << "}";
      const std::string expected = oss.str();
      const auto actual = boost::lexical_cast<std::string>(reader_id);
      ASSERT_EQ(expected, actual);
    }
  });
}

TEST(ReaderIdTest, AllReadersMask) {
  rc::check([](const uint8_t num) {
    if (num > lshl::demux::core::MAX_READER_NUM || num == 0) {
      ASSERT_THROW({ std::ignore = ReaderId::all_readers_mask(num); }, std::invalid_argument);
    } else {
      const uint64_t all_mask = ReaderId::all_readers_mask(num);
      ASSERT_EQ(pow(2, num) - 1, all_mask);
    }
  });
}

TEST(ReaderIdTest, ReaderIdGenerator) {
  rc::check([](const ReaderId& sub) { RC_TAG(sub.mask()); });
}

// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)
