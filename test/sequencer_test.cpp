#include "../src/sequencer.h"
#include <gtest/gtest.h>
#include <sstream>

TEST(sequencer_test, print_status_2) {
  std::ostringstream out;
  ShmSequencer::Sequencer sut(2);
  sut.print_status(out);
  EXPECT_STREQ(out.str().c_str(), "Sequencer{downstream_sequence_number=1,upstream_sequence_numbers=[1,1]}");
}

TEST(sequencer_test, print_status_3) {
  std::ostringstream out;
  ShmSequencer::Sequencer sut(3);
  sut.print_status(out);
  EXPECT_STREQ(out.str().c_str(), "Sequencer{downstream_sequence_number=1,upstream_sequence_numbers=[1,1,1]}");
}

TEST(sequencer_test, print_status_5) {
  std::ostringstream out;
  ShmSequencer::Sequencer sut(5);
  sut.print_status(out);
  EXPECT_STREQ(out.str().c_str(), "Sequencer{downstream_sequence_number=1,upstream_sequence_numbers=[1,1,1,1,1]}");
}

TEST(sequencer_test, print_sequencer_error) {
  std::ostringstream out;

  out << ShmSequencer::SequencerError::SharedMemoryCreate;
  EXPECT_STREQ(out.str().c_str(), "SequencerError::SharedMemoryCreate");

  out.str("");
  out.clear();

  out << ShmSequencer::SequencerError::SharedMemoryWrite;
  EXPECT_STREQ(out.str().c_str(), "SequencerError::SharedMemoryWrite");

  out.str("");
  out.clear();

  out << ShmSequencer::SequencerError::SharedMemoryRead;
  EXPECT_STREQ(out.str().c_str(), "SequencerError::SharedMemoryRead");
}