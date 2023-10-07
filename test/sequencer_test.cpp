#include "../src/sequencer.h"
#include <gtest/gtest.h>
#include <sstream>

TEST(SequencerTest, PrintStatus2) {
  ShmSequencer::Sequencer const sut(2, "abcd");
  EXPECT_EQ(2, sut.client_number());
  std::ostringstream out;
  out << sut;
  EXPECT_STREQ(out.str().c_str(),
               "Sequencer{MaxMsgSize=1024,DownstreamQueueSize=256,session_name=abcd,downstream_sequence_number=1"
               ",upstream_sequence_numbers=[1,1]}");
}

TEST(SequencerTest, PrintStatus3) {
  ShmSequencer::Sequencer<128, 16> const sut(3, "dummy1");
  EXPECT_EQ(3, sut.client_number());
  std::ostringstream out;
  out << sut;
  EXPECT_STREQ(out.str().c_str(),
               "Sequencer{MaxMsgSize=128,DownstreamQueueSize=16,session_name=dummy1,downstream_sequence_number=1"
               ",upstream_sequence_numbers=[1,1,1]}");
}

TEST(SequencerTest, PrintStatus5) {
  ShmSequencer::Sequencer<256, 32> const sut(5, "dummy2");
  EXPECT_EQ(5, sut.client_number());
  std::ostringstream out;
  out << sut;
  EXPECT_STREQ(out.str().c_str(),
               "Sequencer{MaxMsgSize=256,DownstreamQueueSize=32,session_name=dummy2,downstream_sequence_number=1"
               ",upstream_sequence_numbers=[1,1,1,1,1]}");
}

TEST(SequencerTest, PrintSequenceError) {
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

  out.str("");
  out.clear();

  out << ShmSequencer::SequencerError::Unexpected;
  EXPECT_STREQ(out.str().c_str(), "SequencerError::Unexpected");
}