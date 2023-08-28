#ifndef __SEQUENCER_H__
#define __SEQUENCER_H__

#include <variant>
#include <optional>
#include <vector>
#include <ostream>
#include <algorithm>

using std::uint64_t;

namespace ShmSequencer
{
  enum SequencerError
  {
    SharedMemoryCreate,
    SharedMemoryWrite,
    SharedMemoryRead
  };

  class Sequencer
  {
  public:
    Sequencer(const uint16_t client_num_, const uint32_t queue_size_ = 256) : client_num(client_num_),
                                                                              queue_size(queue_size_),
                                                                              upstream_sequence_numbers(client_num_),
                                                                              downstream_sequence_number(1)
    {
      std::fill(this->upstream_sequence_numbers.begin(), this->upstream_sequence_numbers.end(), 1);
    };

    auto start() noexcept -> std::optional<SequencerError>;
    auto stop() noexcept -> std::optional<SequencerError>;
    auto print_status(std::ostream &output) const noexcept -> void;

  private:
    const uint16_t client_num;
    const uint32_t queue_size;
    std::vector<uint64_t> upstream_sequence_numbers;
    uint64_t downstream_sequence_number;
  };
}

#endif // __SEQUENCER_H__