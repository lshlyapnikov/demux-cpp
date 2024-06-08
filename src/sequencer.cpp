#include "./sequencer.h"
#include <iostream>

namespace demux {

auto operator<<(std::ostream& os, const SequencerError& obj) -> std::ostream& {
  switch (obj) {
    case SharedMemoryCreate:
      os << "SequencerError::SharedMemoryCreate";
      break;
    case SharedMemoryWrite:
      os << "SequencerError::SharedMemoryWrite";
      break;
    case SharedMemoryRead:
      os << "SequencerError::SharedMemoryRead";
      break;
    case Unexpected:
      os << "SequencerError::Unexpected";
      break;
  }
  return os;
}

}  // namespace demux