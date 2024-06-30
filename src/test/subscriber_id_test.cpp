// Copyright 2024 Leonid Shlyapnikov.
// SPDX-License-Identifier: Apache-2.0

// NOLINTBEGIN(readability-function-cognitive-complexity, misc-include-cleaner)

#define UNIT_TEST

#include "../demux/subscriber_id.h"
#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <tuple>
#include "./subscriber_id_test.h"

using lshl::demux::SubscriberId;

TEST(DomainTest, Constants) {
  EXPECT_EQ(lshl::demux::MAX_SUBSCRIBER_NUM, 64);
}

TEST(DomainTest, SubscriberIdManualCheck) {
  for (const uint8_t num :
       {static_cast<uint8_t>(0), static_cast<uint8_t>(65), static_cast<uint8_t>(128), static_cast<uint8_t>(255)}) {
    ASSERT_THROW({ std::ignore = SubscriberId::create(num); }, std::invalid_argument);
    ASSERT_THROW({ std::ignore = SubscriberId::all_subscribers_mask(num); }, std::invalid_argument);
  }
  {
    const SubscriberId sub_id = SubscriberId::create(1);
    ASSERT_EQ(0, sub_id.index());
    ASSERT_EQ(0b1, sub_id.mask());

    const uint64_t mask = SubscriberId::all_subscribers_mask(1);
    ASSERT_EQ(0b1, mask);
  }
  {
    const SubscriberId sub_id = SubscriberId::create(2);
    ASSERT_EQ(1, sub_id.index());
    ASSERT_EQ(0b10, sub_id.mask());

    const uint64_t mask = SubscriberId::all_subscribers_mask(2);
    ASSERT_EQ(0b11, mask);
  }
  {
    const SubscriberId sub_id = SubscriberId::create(3);
    ASSERT_EQ(2, sub_id.index());
    ASSERT_EQ(0b100, sub_id.mask());

    const uint64_t mask = SubscriberId::all_subscribers_mask(3);
    ASSERT_EQ(0b111, mask);
  }
  {
    const SubscriberId sub_id = SubscriberId::create(4);
    ASSERT_EQ(3, sub_id.index());
    ASSERT_EQ(0b1000, sub_id.mask());

    const uint64_t mask = SubscriberId::all_subscribers_mask(4);
    ASSERT_EQ(0b1111, mask);
  }
  {
    const SubscriberId sub_id = SubscriberId::create(31);
    ASSERT_EQ(30, sub_id.index());
    ASSERT_EQ(0b1000000000000000000000000000000, sub_id.mask());

    const uint64_t mask = SubscriberId::all_subscribers_mask(31);
    ASSERT_EQ(0b1111111111111111111111111111111, mask);
  }
  {
    const SubscriberId sub_id = SubscriberId::create(32);
    ASSERT_EQ(31, sub_id.index());
    ASSERT_EQ(0b10000000000000000000000000000000, sub_id.mask());

    const uint64_t mask = SubscriberId::all_subscribers_mask(32);
    ASSERT_EQ(0b11111111111111111111111111111111, mask);
  }
  {
    const SubscriberId sub_id = SubscriberId::create(64);
    ASSERT_EQ(63, sub_id.index());
    ASSERT_EQ(0b1000000000000000000000000000000000000000000000000000000000000000, sub_id.mask());

    const uint64_t mask = SubscriberId::all_subscribers_mask(64);
    ASSERT_EQ(0b1111111111111111111111111111111111111111111111111111111111111111, mask);
  }
}

TEST(DomainTest, SubscriberId) {
  rc::check([](const uint8_t num) {
    if (num > lshl::demux::MAX_SUBSCRIBER_NUM || num == 0) {
      ASSERT_THROW({ std::ignore = SubscriberId::create(num); }, std::invalid_argument);
    } else {
      const SubscriberId sub_id = SubscriberId::create(num);
      ASSERT_EQ(num - 1, sub_id.index());
      ASSERT_EQ(pow(2, (num - 1)), sub_id.mask());
    }
  });
}

TEST(DomainTest, AllSubscribersMask) {
  rc::check([](const uint8_t num) {
    if (num > lshl::demux::MAX_SUBSCRIBER_NUM || num == 0) {
      ASSERT_THROW({ std::ignore = SubscriberId::all_subscribers_mask(num); }, std::invalid_argument);
    } else {
      const uint64_t all_mask = SubscriberId::all_subscribers_mask(num);
      ASSERT_EQ(pow(2, num) - 1, all_mask);
    }
  });
}

TEST(DomainTest, SubscriberIdGenerator) {
  rc::check([](const SubscriberId& sub) { RC_TAG(sub.index()); });
}

// NOLINTEND(readability-function-cognitive-complexity, misc-include-cleaner)