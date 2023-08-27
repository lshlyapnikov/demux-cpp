#include "./schema_generated.h"
#include "./sequencer.h"
#include <boost/interprocess/shared_memory_object.hpp>
#include <iostream>

using ShmSequencer::Sequencer;

auto main() -> int
{
  Sequencer<3> sequencer;
  sequencer.start();
  sequencer.stop();

  sequencer.print_status(std::cout);

  return 0;
}