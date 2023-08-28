#include "../src/sequencer.h"
#include <gtest/gtest.h>
#include <sstream>

TEST(sequencer_test, print_status_2)
{
  std::ostringstream out;
  ShmSequencer::Sequencer<2> sut;
  sut.print_status(out);
  EXPECT_STREQ(out.str().c_str(), "Sequencer{downstream_sequence_number=1,upstream_sequence_numbers=[1,1]}");
}

TEST(sequencer_test, print_status_3)
{
  std::ostringstream out;
  ShmSequencer::Sequencer<3> sut;
  sut.print_status(out);
  EXPECT_STREQ(out.str().c_str(), "Sequencer{downstream_sequence_number=1,upstream_sequence_numbers=[1,1,1]}");
}

TEST(sequencer_test, print_status_5)
{
  std::ostringstream out;
  ShmSequencer::Sequencer<5> sut;
  sut.print_status(out);
  EXPECT_STREQ(out.str().c_str(), "Sequencer{downstream_sequence_number=1,upstream_sequence_numbers=[1,1,1,1,1]}");
}
