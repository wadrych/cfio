/// @file test_qt_chart_geometry.cpp
/// @brief Unit tests for the Qt free chart geometry helpers

#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include "display/qt/qt_chart_geometry.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

constexpr double kEps = 1e-9;

MetricsSnapshot MakeSnapshot(int second, uint64_t iops, uint64_t bandwidth, uint64_t p50,
                             uint64_t p95, uint64_t p99) {
  MetricsSnapshot snapshot;
  snapshot.timestamp = std::chrono::steady_clock::time_point{} + std::chrono::seconds(second);
  snapshot.aggregate.job_name = "TOTAL";
  snapshot.aggregate.iops_instant = iops;
  snapshot.aggregate.bw_instant = bandwidth;
  snapshot.aggregate.lat_p50_ns = p50;
  snapshot.aggregate.lat_p95_ns = p95;
  snapshot.aggregate.lat_p99_ns = p99;
  return snapshot;
}

ChartRect MakePlot() {
  return ChartRect{50.0, 10.0, 200.0, 100.0};
}

TEST(NiceTickStepTest, PicksOneTwoOrFive) {
  EXPECT_NEAR(NiceTickStep(50.0, 5), 10.0, kEps);
  EXPECT_NEAR(NiceTickStep(100.0, 5), 20.0, kEps);
  EXPECT_NEAR(NiceTickStep(250.0, 5), 50.0, kEps);
  EXPECT_NEAR(NiceTickStep(400.0, 5), 100.0, kEps);
}

TEST(NiceTickStepTest, ScalesAcrossMagnitudes) {
  EXPECT_NEAR(NiceTickStep(0.005, 5), 0.001, kEps);
  EXPECT_NEAR(NiceTickStep(5.0, 5), 1.0, kEps);
  EXPECT_NEAR(NiceTickStep(500000.0, 5), 100000.0, kEps);
}

TEST(NiceTickStepTest, RejectsBadInput) {
  EXPECT_NEAR(NiceTickStep(0.0, 5), 1.0, kEps);
  EXPECT_NEAR(NiceTickStep(-10.0, 5), 1.0, kEps);
  EXPECT_NEAR(NiceTickStep(50.0, 0), 10.0, kEps);
  EXPECT_NEAR(NiceTickStep(50.0, -3), 10.0, kEps);
}

TEST(MakeAxisScaleTest, RoundsBoundsOutwards) {
  const AxisScale scale = MakeAxisScale(0.0, 137.0, 5);

  EXPECT_NEAR(scale.tick_step, 50.0, kEps);
  EXPECT_NEAR(scale.min, 0.0, kEps);
  EXPECT_NEAR(scale.max, 150.0, kEps);
}

TEST(MakeAxisScaleTest, HandlesNonZeroMinimum) {
  const AxisScale scale = MakeAxisScale(1130.0, 2470.0, 5);

  EXPECT_NEAR(scale.tick_step, 500.0, kEps);
  EXPECT_NEAR(scale.min, 1000.0, kEps);
  EXPECT_NEAR(scale.max, 2500.0, kEps);
}

TEST(MakeAxisScaleTest, AllZeroValuesGiveUnitRange) {
  const AxisScale scale = MakeAxisScale(0.0, 0.0, 5);

  EXPECT_NEAR(scale.min, 0.0, kEps);
  EXPECT_GT(scale.max, scale.min);
  EXPECT_GT(scale.tick_step, 0.0);
}

TEST(MakeAxisScaleTest, SinglePositiveValueStartsAtZero) {
  const AxisScale scale = MakeAxisScale(500.0, 500.0, 5);

  EXPECT_NEAR(scale.min, 0.0, kEps);
  EXPECT_GE(scale.max, 500.0);
  EXPECT_GT(scale.tick_step, 0.0);
}

TEST(MakeAxisScaleTest, SwapsInvertedBounds) {
  const AxisScale scale = MakeAxisScale(300.0, 100.0, 5);

  EXPECT_NEAR(scale.min, 100.0, kEps);
  EXPECT_NEAR(scale.max, 300.0, kEps);
}

TEST(AxisTicksTest, WalksMinToMax) {
  const std::vector<double> ticks = AxisTicks(AxisScale{0.0, 150.0, 50.0});

  ASSERT_EQ(ticks.size(), 4U);
  EXPECT_NEAR(ticks.front(), 0.0, kEps);
  EXPECT_NEAR(ticks[1], 50.0, kEps);
  EXPECT_NEAR(ticks.back(), 150.0, kEps);
}

TEST(AxisTicksTest, RejectsBadScale) {
  EXPECT_TRUE(AxisTicks(AxisScale{0.0, 100.0, 0.0}).empty());
  EXPECT_TRUE(AxisTicks(AxisScale{100.0, 0.0, 10.0}).empty());
}

TEST(MapTest, MapsEndpointsToPlotEdges) {
  const ChartRect plot = MakePlot();
  const AxisScale scale{0.0, 100.0, 20.0};

  EXPECT_NEAR(MapX(0.0, scale, plot), 50.0, kEps);
  EXPECT_NEAR(MapX(100.0, scale, plot), 250.0, kEps);
  EXPECT_NEAR(MapX(50.0, scale, plot), 150.0, kEps);
}

TEST(MapTest, VerticalAxisGrowsUpwards) {
  const ChartRect plot = MakePlot();
  const AxisScale scale{0.0, 100.0, 20.0};

  EXPECT_NEAR(MapY(0.0, scale, plot), 110.0, kEps);
  EXPECT_NEAR(MapY(100.0, scale, plot), 10.0, kEps);
  EXPECT_NEAR(MapY(25.0, scale, plot), 85.0, kEps);
}

TEST(MapTest, ClampsOutOfRangeValues) {
  const ChartRect plot = MakePlot();
  const AxisScale scale{0.0, 100.0, 20.0};

  EXPECT_NEAR(MapX(-10.0, scale, plot), 50.0, kEps);
  EXPECT_NEAR(MapX(500.0, scale, plot), 250.0, kEps);
  EXPECT_NEAR(MapY(-10.0, scale, plot), 110.0, kEps);
  EXPECT_NEAR(MapY(500.0, scale, plot), 10.0, kEps);
}

TEST(MapTest, DegenerateScaleCollapsesToOrigin) {
  const ChartRect plot = MakePlot();
  const AxisScale scale{5.0, 5.0, 1.0};

  EXPECT_NEAR(MapX(5.0, scale, plot), 50.0, kEps);
  EXPECT_NEAR(MapY(5.0, scale, plot), 110.0, kEps);
}

TEST(MapPointsTest, MapsEveryPoint) {
  const ChartRect plot = MakePlot();
  const AxisScale axis{0.0, 100.0, 20.0};
  const std::vector<ChartPoint> points{{0.0, 0.0}, {100.0, 100.0}};

  const std::vector<ChartPoint> mapped = MapPoints(points, axis, axis, plot);

  ASSERT_EQ(mapped.size(), 2U);
  EXPECT_NEAR(mapped[0].x, 50.0, kEps);
  EXPECT_NEAR(mapped[0].y, 110.0, kEps);
  EXPECT_NEAR(mapped[1].x, 250.0, kEps);
  EXPECT_NEAR(mapped[1].y, 10.0, kEps);
}

TEST(MapPointsTest, EmptyInputGivesEmptyOutput) {
  EXPECT_TRUE(MapPoints({}, AxisScale{}, AxisScale{}, MakePlot()).empty());
}

TEST(ExtractSeriesTest, ReadsAggregateMetricOverTime) {
  const std::vector<MetricsSnapshot> history{MakeSnapshot(10, 1000, 4096, 100, 200, 300),
                                             MakeSnapshot(11, 2000, 8192, 110, 220, 330),
                                             MakeSnapshot(13, 3000, 16384, 120, 240, 360)};

  const ChartSeries iops = ExtractSeries(history, ChartMetric::kIops);

  EXPECT_EQ(iops.label, "IOPS");
  ASSERT_EQ(iops.points.size(), 3U);
  EXPECT_NEAR(iops.points[0].x, 0.0, kEps);
  EXPECT_NEAR(iops.points[1].x, 1.0, kEps);
  EXPECT_NEAR(iops.points[2].x, 3.0, kEps);
  EXPECT_NEAR(iops.points[0].y, 1000.0, kEps);
  EXPECT_NEAR(iops.points[2].y, 3000.0, kEps);
}

TEST(ExtractSeriesTest, ReadsEachMetric) {
  const std::vector<MetricsSnapshot> history{MakeSnapshot(0, 1000, 4096, 100, 200, 300)};

  EXPECT_NEAR(ExtractSeries(history, ChartMetric::kBandwidth).points[0].y, 4096.0, kEps);
  EXPECT_NEAR(ExtractSeries(history, ChartMetric::kLatencyP50).points[0].y, 100.0, kEps);
  EXPECT_NEAR(ExtractSeries(history, ChartMetric::kLatencyP95).points[0].y, 200.0, kEps);
  EXPECT_NEAR(ExtractSeries(history, ChartMetric::kLatencyP99).points[0].y, 300.0, kEps);
  EXPECT_EQ(ExtractSeries(history, ChartMetric::kLatencyP99).label, "p99");
}

TEST(ExtractSeriesTest, EmptyHistoryGivesEmptySeries) {
  const ChartSeries series = ExtractSeries({}, ChartMetric::kIops);

  EXPECT_EQ(series.label, "IOPS");
  EXPECT_TRUE(series.points.empty());
}

TEST(SeriesDurationSecondsTest, MeasuresFirstToLast) {
  const std::vector<MetricsSnapshot> history{MakeSnapshot(5, 0, 0, 0, 0, 0),
                                             MakeSnapshot(35, 0, 0, 0, 0, 0)};

  EXPECT_NEAR(SeriesDurationSeconds(history), 30.0, kEps);
  EXPECT_NEAR(SeriesDurationSeconds({}), 0.0, kEps);
  EXPECT_NEAR(SeriesDurationSeconds({MakeSnapshot(5, 0, 0, 0, 0, 0)}), 0.0, kEps);
}

TEST(MaxYTest, ScansEverySeries) {
  const ChartSeries first{"p50", {{0.0, 10.0}, {1.0, 40.0}}};
  const ChartSeries second{"p99", {{0.0, 90.0}, {1.0, 20.0}}};

  EXPECT_NEAR(MaxY({first, second}), 90.0, kEps);
  EXPECT_NEAR(MaxY({}), 0.0, kEps);
  EXPECT_NEAR(MaxY({ChartSeries{"empty", {}}}), 0.0, kEps);
}

TEST(MaxYTest, AllZeroSeriesStillScales) {
  const ChartSeries flat{"IOPS", {{0.0, 0.0}, {1.0, 0.0}}};
  const AxisScale scale = MakeAxisScale(0.0, MaxY({flat}), kDefaultTickCount);

  EXPECT_GT(scale.max, scale.min);
  EXPECT_FALSE(AxisTicks(scale).empty());
}

}  // namespace
}  // namespace cfio
