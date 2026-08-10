/// @file test_metric_format.cpp
/// @brief Unit tests for the shared metric formatting helpers

#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include "display/metric_format.h"

namespace cfio {
namespace {

TEST(MetricFormatTest, FormatCountBelowThousandHasNoSeparator) {
  EXPECT_EQ(FormatCount(0), "0");
  EXPECT_EQ(FormatCount(7), "7");
  EXPECT_EQ(FormatCount(999), "999");
}

TEST(MetricFormatTest, FormatCountInsertsSeparators) {
  EXPECT_EQ(FormatCount(1000), "1,000");
  EXPECT_EQ(FormatCount(125432), "125,432");
  EXPECT_EQ(FormatCount(7092000), "7,092,000");
}

TEST(MetricFormatTest, FormatCountHandlesMaxValue) {
  EXPECT_EQ(FormatCount(std::numeric_limits<std::uint64_t>::max()), "18,446,744,073,709,551,615");
}

TEST(MetricFormatTest, FormatBytesUsesRawBytesBelowKiB) {
  EXPECT_EQ(FormatBytes(0), "0 B");
  EXPECT_EQ(FormatBytes(512), "512 B");
  EXPECT_EQ(FormatBytes(kBytesPerKiB - 1), "1023 B");
}

TEST(MetricFormatTest, FormatBytesPicksLargestFittingUnit) {
  EXPECT_EQ(FormatBytes(kBytesPerKiB), "1.0 KiB");
  EXPECT_EQ(FormatBytes(kBytesPerMiB - 1), "1024.0 KiB");
  EXPECT_EQ(FormatBytes(kBytesPerMiB), "1.0 MiB");
  EXPECT_EQ(FormatBytes(kBytesPerGiB), "1.0 GiB");
  EXPECT_EQ(FormatBytes(27 * kBytesPerGiB), "27.0 GiB");
}

TEST(MetricFormatTest, RateMiBTruncatesTowardsZero) {
  EXPECT_EQ(RateMiB(0), 0U);
  EXPECT_EQ(RateMiB(kBytesPerMiB - 1), 0U);
  EXPECT_EQ(RateMiB(kBytesPerMiB), 1U);
  EXPECT_EQ(RateMiB(512 * kBytesPerMiB), 512U);
}

TEST(MetricFormatTest, LatencyUsTruncatesTowardsZero) {
  EXPECT_EQ(LatencyUs(0), 0U);
  EXPECT_EQ(LatencyUs(999), 0U);
  EXPECT_EQ(LatencyUs(1000), 1U);
  EXPECT_EQ(LatencyUs(45000), 45U);
}

TEST(MetricFormatTest, FormatRateAppendsTheUnit) {
  EXPECT_EQ(FormatRate(0), "0 MB/s");
  EXPECT_EQ(FormatRate(kBytesPerKiB), "0 MB/s");
  EXPECT_EQ(FormatRate(512 * kBytesPerMiB), "512 MB/s");
}

TEST(MetricFormatTest, FormatLatencyUsAppendsTheUnit) {
  EXPECT_EQ(FormatLatencyUs(0), "0 μs");
  EXPECT_EQ(FormatLatencyUs(999), "0 μs");
  EXPECT_EQ(FormatLatencyUs(45000), "45 μs");
}

TEST(MetricFormatTest, FormatDurationRendersMinutesAndSeconds) {
  EXPECT_EQ(FormatDuration(0), "00:00");
  EXPECT_EQ(FormatDuration(15), "00:15");
  EXPECT_EQ(FormatDuration(60), "01:00");
  EXPECT_EQ(FormatDuration(3599), "59:59");
}

TEST(MetricFormatTest, FormatDurationClampsNegativeInput) {
  EXPECT_EQ(FormatDuration(-5), "00:00");
}

}  // namespace
}  // namespace cfio
