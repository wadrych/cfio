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
