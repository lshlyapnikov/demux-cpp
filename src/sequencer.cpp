#include "./sequencer.h"

namespace ShmSequencer
{
  auto Sequencer::start() noexcept -> std::optional<SequencerError>
  {
    return std::nullopt;
  }

  auto Sequencer::stop() noexcept -> std::optional<SequencerError>
  {
    return std::nullopt;
  }

  auto Sequencer::print_status(std::ostream &output) const noexcept -> void
  {
    output << "Sequencer{downstream_sequence_number=" << this->downstream_sequence_number
           << ",upstream_sequence_numbers=[";
    bool first = true;
    for (const uint64_t &x : this->upstream_sequence_numbers)
    {
      if (first)
      {
        output << x;
        first = false;
      }
      else
      {
        output << "," << x;
      }
    }
    output << "]}";
  }
} // namespace ShmSequencer