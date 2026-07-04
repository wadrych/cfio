/// @file test_log2_histogram.cpp
/// @brief Unit tests for the Log2Histogram
///

#include <cstdint>

#include <gtest/gtest.h>

#include "telemetry/log2_histogram.h"

namespace cfio {
namespace {

TEST(Log2HistogramTest, SingleValueRecordAndPercentile) {
  Log2Histogram<> h;
  h.Record(100);

  EXPECT_EQ(h.TotalCount(), 1u);
  EXPECT_EQ(h.Percentile(0.5), 127u);
  EXPECT_EQ(h.Percentile(0.0), 127u);
}

TEST(Log2HistogramTest, PercentilesWithKnownDistribution) {
  Log2Histogram<> h;
  for (int i = 0; i < 50; ++i) {
    h.Record(8);
  }
  for (int i = 0; i < 45; ++i) {
    h.Record(16);
  }
  for (int i = 0; i < 5; ++i) {
    h.Record(32);
  }

  ASSERT_EQ(h.TotalCount(), 100u);
  EXPECT_EQ(h.Percentile(0.50), 15u);
  EXPECT_EQ(h.Percentile(0.95), 31u);
  EXPECT_EQ(h.Percentile(0.99), 63u);
  EXPECT_EQ(h.Percentile(1.0), 63u);
}


TEST(Log2HistogramTest, MinAndMaxValue) {
  Log2Histogram<> h;
  EXPECT_EQ(h.MinValue(), 0u);
  EXPECT_EQ(h.MaxValue(), 0u);

  h.Record(8);
  h.Record(100);
  EXPECT_EQ(h.MinValue(), 8u);
  EXPECT_EQ(h.MaxValue(), 127u);
}

TEST(Log2HistogramTest, MergeTwoHistograms) {
  Log2Histogram<> h1;
  Log2Histogram<> h2;
  for (int i = 0; i < 50; ++i) {
    h1.Record(8);
    h2.Record(32);
  }

  h1.Merge(h2);

  EXPECT_EQ(h1.TotalCount(), 100u);
  EXPECT_EQ(h1.MinValue(), 8u);
  EXPECT_EQ(h1.MaxValue(), 63u);
  EXPECT_EQ(h1.Percentile(0.5), 15u);

  EXPECT_EQ(h2.TotalCount(), 50u);
}

TEST(Log2HistogramTest, ResetClearsAll) {
  Log2Histogram<> h;
  h.Record(8);
  h.Record(100);
  h.Record(4096);

  h.Reset();

  EXPECT_EQ(h.TotalCount(), 0u);
  EXPECT_EQ(h.Percentile(0.5), 0u);
  EXPECT_EQ(h.MinValue(), 0u);
  EXPECT_EQ(h.MaxValue(), 0u);
}

TEST(Log2HistogramTest, EdgeCases) {
  Log2Histogram<> zero_hist;
  zero_hist.Record(0);
  EXPECT_EQ(zero_hist.TotalCount(), 1u);
  EXPECT_EQ(zero_hist.MinValue(), 0u);
  EXPECT_EQ(zero_hist.MaxValue(), 1u);
  EXPECT_EQ(zero_hist.Percentile(1.0), 1u);

  Log2Histogram<> max_hist;
  max_hist.Record(UINT64_MAX);
  EXPECT_EQ(max_hist.MaxValue(), UINT64_MAX);
  EXPECT_EQ(max_hist.Percentile(1.0), UINT64_MAX);

  Log2Histogram<> empty_hist;
  EXPECT_EQ(empty_hist.Percentile(0.5), 0u);
}

TEST(Log2HistogramTest, NonDefaultTemplateParams) {
  Log2Histogram<32, std::uint32_t> h;

  h.Record(8);
  EXPECT_EQ(h.MinValue(), 8u);

  h.Record(1ULL << 40);
  EXPECT_EQ(h.MaxValue(), (1ULL << 32) - 1);

  h.Record(UINT64_MAX);
  EXPECT_EQ(h.MaxValue(), (1ULL << 32) - 1);

  EXPECT_EQ(h.TotalCount(), 3u);
}

}  // namespace
}  // namespace cfio
