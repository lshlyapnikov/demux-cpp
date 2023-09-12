#ifndef __SHM_SEQUENCER_SEQUENCER_H__
#define __SHM_SEQUENCER_SEQUENCER_H__

#include <algorithm>
#include <optional>
#include <ostream>
#include <variant>
#include <vector>

using std::uint64_t;

namespace ShmSequencer {

enum SequencerError { SharedMemoryCreate, SharedMemoryWrite, SharedMemoryRead };

class Sequencer {
 public:
  explicit Sequencer(const size_t client_num_,
                     const size_t max_message_size_ = 1024,
                     const uint32_t downstream_queue_size_ = 256)
      : client_num(client_num_),
        max_message_size(max_message_size_),
        downstream_queue_size(downstream_queue_size_),
        upstream_sequence_numbers(client_num_),
        downstream_sequence_number(1) {
    std::fill(this->upstream_sequence_numbers.begin(), this->upstream_sequence_numbers.end(), 1);
  };

  [[nodiscard]] auto start() noexcept -> std::optional<SequencerError>;
  [[nodiscard]] auto stop() noexcept -> std::optional<SequencerError>;
  auto print_status(std::ostream& output) const noexcept -> void;

 private:
  const size_t client_num;
  const size_t max_message_size;
  const uint32_t downstream_queue_size;
  std::vector<uint64_t> upstream_sequence_numbers;
  uint64_t downstream_sequence_number;
};

auto operator<<(std::ostream& os, const SequencerError& obj) -> std::ostream&;

auto operator<<(std::ostream& os, const Sequencer& obj) -> std::ostream&;

}  // namespace ShmSequencer

#endif  // __SHM_SEQUENCER_SEQUENCER_H__