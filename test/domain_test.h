#ifndef SHM_SEQUENCER_DOMAIN_TEST_H
#define SHM_SEQUENCER_DOMAIN_TEST_H

#include <rapidcheck.h>  // NOLINT(misc-include-cleaner)
#include "../src/domain.h"

namespace rc {

template <>
struct Arbitrary<ShmSequencer::SubscriberId> {
  static auto arbitrary() -> Gen<ShmSequencer::SubscriberId> {
    using ShmSequencer::is_valid_subscriber_number;
    using ShmSequencer::MAX_SUBSCRIBER_NUM;
    using ShmSequencer::SubscriberId;

    const Gen<uint8_t> range = gen::inRange(static_cast<uint8_t>(1), MAX_SUBSCRIBER_NUM);
    const Gen<uint8_t> filtered = gen::suchThat(range, is_valid_subscriber_number);
    return gen::map(filtered, SubscriberId::create);
  }
};

}  // namespace rc

#endif  // SHM_SEQUENCER_DOMAIN_TEST_H
