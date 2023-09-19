#ifndef SHM_SEQUENCER_SEQUENCER_H
#define SHM_SEQUENCER_SEQUENCER_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <vector>

namespace ShmSequencer {

using std::optional;
using std::size_t;
using std::uint64_t;

const size_t DEFAULT_MAX_MSG_SIZE = 1024;
const size_t DEFAULT_DOWNSTREAM_QUEUE_SIZE = 256;

enum SequencerError { SharedMemoryCreate, SharedMemoryWrite, SharedMemoryRead, Unexpected };

auto operator<<(std::ostream& os, const SequencerError& obj) -> std::ostream&;

template <const size_t MaxMsgSize = DEFAULT_MAX_MSG_SIZE,
          const size_t DownstreamQueueSize = DEFAULT_DOWNSTREAM_QUEUE_SIZE>
class Sequencer {
 public:
  explicit Sequencer(const size_t client_num) : upstream_sequence_numbers_(client_num) {
    std::fill(this->upstream_sequence_numbers_.begin(), this->upstream_sequence_numbers_.end(), 1);
  };

  [[nodiscard]] auto start() noexcept -> optional<SequencerError> { return std::nullopt; }
  [[nodiscard]] auto stop() noexcept -> optional<SequencerError> { return {SequencerError::Unexpected}; }
  [[nodiscard]] auto client_number() const noexcept -> size_t { return this->upstream_sequence_numbers_.size(); }

  auto print_status(std::ostream& output) const -> std::ostream& {
    output << "Sequencer{MaxMsgSize=" << MaxMsgSize << ",DownstreamQueueSize=" << DownstreamQueueSize
           << ",downstream_sequence_number=" << this->downstream_sequence_number_ << ",upstream_sequence_numbers=[";
    bool first = true;
    for (const uint64_t& x : this->upstream_sequence_numbers_) {
      if (first) {
        output << x;
        first = false;
      } else {
        output << "," << x;
      }
    }
    output << "]}";
    return output;
  }

 private:
  std::vector<uint64_t> upstream_sequence_numbers_;
  uint64_t downstream_sequence_number_{1};
};

template <const size_t MaxMsgSize = DEFAULT_MAX_MSG_SIZE,
          const size_t DownstreamQueueSize = DEFAULT_DOWNSTREAM_QUEUE_SIZE>
auto operator<<(std::ostream& output, const Sequencer<MaxMsgSize, DownstreamQueueSize>& obj) -> std::ostream& {
  return obj.print_status(output);
}

}  // namespace ShmSequencer

#endif  // SHM_SEQUENCER_SEQUENCER_H