#ifndef SHM_SEQUENCER_DOMAIN_TEST_H
#define SHM_SEQUENCER_DOMAIN_TEST_H

#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include "../src/domain.h"

namespace rc {

template <>
struct Arbitrary<demux::SubscriberId> {
  static auto arbitrary() -> Gen<demux::SubscriberId> {
    using demux::is_valid_subscriber_number;
    using demux::MAX_SUBSCRIBER_NUM;
    using demux::SubscriberId;

    const Gen<uint8_t> range = gen::inRange(static_cast<uint8_t>(1), MAX_SUBSCRIBER_NUM);
    const Gen<uint8_t> filtered = gen::suchThat(range, is_valid_subscriber_number);
    return gen::map(filtered, SubscriberId::create);
  }
};

}  // namespace rc

#endif  // SHM_SEQUENCER_DOMAIN_TEST_H
