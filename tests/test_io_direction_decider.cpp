/// @file test_io_direction_decider.cpp
/// @brief Unit tests for the IODirectionDecider
///

#include <cstdint>

#include <gtest/gtest.h>

#include "common/types.h"
#include "telemetry/io_direction_decider.h"

namespace cfio {
namespace {

constexpr std::uint64_t kSeed = 987654321;

TEST(IODirectionDeciderTest, PureReadAlwaysRead) {
  IODirectionDecider seq(RWMode::kRead, 0, kSeed);
  IODirectionDecider rnd(RWMode::kRandRead, 50, kSeed);

  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(seq.Next(), IODirection::kRead);
    EXPECT_EQ(rnd.Next(), IODirection::kRead);
  }
}

TEST(IODirectionDeciderTest, PureWriteAlwaysWrite) {
  IODirectionDecider seq(RWMode::kWrite, 100, kSeed);
  IODirectionDecider rnd(RWMode::kRandWrite, 50, kSeed);

  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(seq.Next(), IODirection::kWrite);
    EXPECT_EQ(rnd.Next(), IODirection::kWrite);
  }
}

TEST(IODirectionDeciderTest, MixedRatioApproximate) {
  constexpr int kSamples = 10000;
  constexpr int kReadPercent = 70;

  for (RWMode mode : {RWMode::kReadWrite, RWMode::kRandRW}) {
    IODirectionDecider decider(mode, kReadPercent, kSeed);
    int reads = 0;
    for (int i = 0; i < kSamples; ++i) {
      if (decider.Next() == IODirection::kRead) {
        ++reads;
      }
    }
    const double fraction = static_cast<double>(reads) / kSamples;
    EXPECT_NEAR(fraction, 0.70, 0.03)
        << "unexpected read fraction for mode " << static_cast<int>(mode);
  }
}

TEST(IODirectionDeciderTest, MixedEdgeRatios) {
  IODirectionDecider all_write(RWMode::kReadWrite, 0, kSeed);
  IODirectionDecider all_read(RWMode::kReadWrite, 100, kSeed);

  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(all_write.Next(), IODirection::kWrite);
    EXPECT_EQ(all_read.Next(), IODirection::kRead);
  }
}

}  // namespace
}  // namespace cfio
