#include "./test_message.h"
#include "../util/operators.h"

namespace lshl::demux::core::test {

auto operator<<(std::ostream& os, const TestMessage& message) -> std::ostream& {
  using lshl::demux::util::operator<<;
  return os << message.t;
}

}  // namespace lshl::demux::core::test