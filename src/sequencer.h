#ifndef __SEQUENCER_H__
#define __SEQUENCER_H__

#include <variant>
#include <optional>
#include <array>
#include <ostream>

using std::uint64_t;

namespace ShmSequencer
{
  enum SequencerError
  {
    SharedMemoryCreate,
    SharedMemoryWrite,
    SharedMemoryRead
  };

  template <size_t const N>
  class Sequencer
  {
  public:
    Sequencer() : downstream_sequence_number(1)
    {
      this->upstream_sequence_numbers.fill(1);
    };

    auto start() noexcept -> std::optional<SequencerError>;
    auto stop() noexcept -> std::optional<SequencerError>;
    auto print_status(std::ostream &output) const noexcept -> void;

  private:
    uint64_t downstream_sequence_number;
    std::array<uint64_t, N> upstream_sequence_numbers;
  };

  template <size_t const N>
  auto Sequencer<N>::start() noexcept -> std::optional<SequencerError>
  {
    return std::nullopt;
  }

  template <size_t const N>
  auto Sequencer<N>::stop() noexcept -> std::optional<SequencerError>
  {
    return std::nullopt;
  }

  template <size_t const N>
  auto Sequencer<N>::print_status(std::ostream &output) const noexcept -> void
  {
    output << "Sequencer{downstream_sequence_number=" << this->downstream_sequence_number
           << ",upstream_sequence_numbers=[";
    bool first = true;
    for (const uint &x : this->upstream_sequence_numbers)
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
}

#endif // __SEQUENCER_H__