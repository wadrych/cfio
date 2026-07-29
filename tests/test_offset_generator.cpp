/// @file test_offset_generator.cpp
/// @brief Unit tests for the OffsetGenerator
///

#include <sys/types.h>

#include <cstddef>
#include <cstdint>

#include <gtest/gtest.h>

#include "common/types.h"
#include "telemetry/offset_generator.h"

namespace cfio {
namespace {

constexpr size_t kBlockSize = 4096;
constexpr std::uint64_t kSeed = 12345;

TEST(OffsetGeneratorTest, SequentialAdvancesAndWraps) {
  const size_t file_size = 4 * kBlockSize;
  OffsetGenerator gen(AccessPattern::kSequential, file_size, kBlockSize, kBlockSize, kSeed);

  const auto bs = static_cast<off_t>(kBlockSize);
  for (int cycle = 0; cycle < 2; ++cycle) {
    EXPECT_EQ(gen.Next(), 0);
    EXPECT_EQ(gen.Next(), bs);
    EXPECT_EQ(gen.Next(), 2 * bs);
    EXPECT_EQ(gen.Next(), 3 * bs);
  }
}

TEST(OffsetGeneratorTest, RandomOffsetsAlignedAndInRange) {
  const size_t file_size = 1024 * kBlockSize;
  const auto max_offset = static_cast<off_t>(file_size - kBlockSize);
  OffsetGenerator gen(AccessPattern::kRandom, file_size, kBlockSize, kBlockSize, kSeed);

  for (int i = 0; i < 10000; ++i) {
    const off_t offset = gen.Next();
    EXPECT_GE(offset, 0);
    EXPECT_LE(offset, max_offset);
    EXPECT_EQ(offset % static_cast<off_t>(kBlockSize), 0) << "offset not aligned: " << offset;
  }
}

TEST(OffsetGeneratorTest, RandomRoundsDownToAlignmentSmallerThanBlock) {
  constexpr size_t kAlignment = 4096;
  constexpr size_t kBigBlock = 8192;
  const size_t file_size = 512 * kAlignment;
  OffsetGenerator gen(AccessPattern::kRandom, file_size, kBigBlock, kAlignment, kSeed);

  for (int i = 0; i < 10000; ++i) {
    const off_t offset = gen.Next();
    EXPECT_GE(offset, 0);
    EXPECT_EQ(offset % static_cast<off_t>(kAlignment), 0) << "offset not aligned: " << offset;
    EXPECT_LE(static_cast<size_t>(offset) + kBigBlock, file_size)
        << "block would run past EOF at offset " << offset;
  }
}

TEST(OffsetGeneratorTest, SameSeedProducesSameSequence) {
  const size_t file_size = 1024 * kBlockSize;
  OffsetGenerator first(AccessPattern::kRandom, file_size, kBlockSize, kBlockSize, kSeed);
  OffsetGenerator second(AccessPattern::kRandom, file_size, kBlockSize, kBlockSize, kSeed);

  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(first.Next(), second.Next());
  }
}

TEST(OffsetGeneratorTest, DifferentSeedsDiverge) {
  const size_t file_size = 1024 * kBlockSize;
  OffsetGenerator first(AccessPattern::kRandom, file_size, kBlockSize, kBlockSize, kSeed);
  OffsetGenerator second(AccessPattern::kRandom, file_size, kBlockSize, kBlockSize, kSeed + 1);

  bool any_difference = false;
  for (int i = 0; i < 100 && !any_difference; ++i) {
    any_difference = first.Next() != second.Next();
  }
  EXPECT_TRUE(any_difference);
}

TEST(OffsetGeneratorTest, DefaultSeedStaysInRange) {
  const size_t file_size = 64 * kBlockSize;
  const auto max_offset = static_cast<off_t>(file_size - kBlockSize);
  OffsetGenerator gen(AccessPattern::kRandom, file_size, kBlockSize, kBlockSize);

  for (int i = 0; i < 1000; ++i) {
    const off_t offset = gen.Next();
    EXPECT_GE(offset, 0);
    EXPECT_LE(offset, max_offset);
    EXPECT_EQ(offset % static_cast<off_t>(kBlockSize), 0);
  }
}

TEST(OffsetGeneratorTest, SingleBlockFileAlwaysZero) {
  OffsetGenerator seq(AccessPattern::kSequential, kBlockSize, kBlockSize, kBlockSize, kSeed);
  OffsetGenerator rnd(AccessPattern::kRandom, kBlockSize, kBlockSize, kBlockSize, kSeed);

  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(seq.Next(), 0);
    EXPECT_EQ(rnd.Next(), 0);
  }
}

}  // namespace
}  // namespace cfio
