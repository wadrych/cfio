/// @file test_terminal_display.cpp
/// @brief Unit tests for TerminalDispla

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "common/types.h"
#include "config/job_config.h"
#include "display/display_context.h"
#include "display/display_factory.h"
#include "display/terminal_display.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;
constexpr std::uint64_t kGiB = 1024ULL * 1024ULL * 1024ULL;

DisplayContext SampleContext() {
  return DisplayContext{"io_uring", "ON", "./cfio-results/run/cfio.log"};
}

MetricsSnapshot SampleSnapshot() {
  MetricsSnapshot snapshot;

  PerJobMetrics read;
  read.job_name = "rand-read-4k";
  read.iops_instant = 125432;
  read.iops_cumulative = 118200;
  read.bw_instant = 512 * kMiB;
  read.bw_cumulative = 483 * kMiB;
  read.lat_p50_ns = 8000;
  read.lat_p95_ns = 32000;
  read.lat_p99_ns = 45000;

  PerJobMetrics write;
  write.job_name = "seq-write-128k";
  write.iops_instant = 48291;
  write.iops_cumulative = 51003;
  write.bw_instant = 189 * kMiB;
  write.bw_cumulative = 198 * kMiB;
  write.lat_p50_ns = 22000;
  write.lat_p95_ns = 105000;
  write.lat_p99_ns = 312000;

  snapshot.jobs = {read, write};

  snapshot.aggregate.job_name = "TOTAL";
  snapshot.aggregate.iops_instant = 173723;
  snapshot.aggregate.iops_cumulative = 169203;
  snapshot.aggregate.bw_instant = 701 * kMiB;
  snapshot.aggregate.bw_cumulative = 681 * kMiB;
  snapshot.aggregate.lat_p50_ns = 777000;
  return snapshot;
}

BenchmarkResults SampleResults() {
  BenchmarkResults results;
  results.runtime_seconds = 60;

  JobResults read;
  read.name = "rand-read-4k";
  read.config.rw_mode = RWMode::kRandRead;
  read.config.block_size = 4096;
  read.config.iodepth = 32;
  read.iops_avg = 118200;
  read.bw_avg_bytes = 483 * kMiB;
  read.lat_min_ns = 1000;
  read.lat_p50_ns = 8000;
  read.lat_p95_ns = 32000;
  read.lat_p99_ns = 45000;
  read.lat_max_ns = 892000;
  read.total_ios = 7092000;
  read.total_bytes = 27 * kGiB;

  JobResults write;
  write.name = "seq-write-128k";
  write.config.rw_mode = RWMode::kWrite;
  write.config.block_size = 128ULL * 1024;
  write.config.iodepth = 32;
  write.iops_avg = 51003;
  write.bw_avg_bytes = 198 * kMiB;
  write.lat_min_ns = 4000;
  write.lat_p50_ns = 22000;
  write.lat_p95_ns = 105000;
  write.lat_p99_ns = 312000;
  write.lat_max_ns = 41234000;
  write.total_ios = 95000;
  write.total_bytes = 12 * kGiB;

  results.jobs = {read, write};
  return results;
}

TEST(TerminalDisplayTest, RenderLiveViewShowsMetrics) {
  std::ostringstream sink;
  TerminalDisplay display(SampleContext(), sink);
  display.Init(60);

  const std::string frame = display.RenderLiveView(SampleSnapshot(), 15);

  EXPECT_NE(frame.find("rand-read-4k"), std::string::npos);
  EXPECT_NE(frame.find("125,432"), std::string::npos);
  EXPECT_NE(frame.find("512/483 MB/s"), std::string::npos);
  EXPECT_NE(frame.find("8/32/45 μs"), std::string::npos);
  EXPECT_NE(frame.find("Engine: io_uring"), std::string::npos);
  EXPECT_NE(frame.find("Direct: ON"), std::string::npos);
  EXPECT_NE(frame.find("[00:15 / 01:00]"), std::string::npos);
  EXPECT_NE(frame.find("Errors: R:0 W:0"), std::string::npos);
  EXPECT_NE(frame.find("Log: ./cfio-results/run/cfio.log"), std::string::npos);
  EXPECT_NE(frame.find("TOTAL"), std::string::npos);
  EXPECT_NE(frame.find("173,723"), std::string::npos);
}

TEST(TerminalDisplayTest, TotalRowOmitsLatency) {
  std::ostringstream sink;
  TerminalDisplay display(SampleContext(), sink);
  display.Init(60);

  const std::string frame = display.RenderLiveView(SampleSnapshot(), 15);

  EXPECT_EQ(frame.find("777"), std::string::npos);
}

TEST(TerminalDisplayTest, UpdateWritesFrameToStream) {
  std::ostringstream sink;
  TerminalDisplay display(SampleContext(), sink);

  display.Update(SampleSnapshot());

  const std::string out = sink.str();
  EXPECT_FALSE(out.empty());
  EXPECT_NE(out.find("\033[H"), std::string::npos);
  EXPECT_NE(out.find("seq-write-128k"), std::string::npos);
}

TEST(TerminalDisplayTest, ShutdownRestoresCursor) {
  std::ostringstream sink;
  TerminalDisplay display(SampleContext(), sink);
  display.Init(30);
  sink.str("");
  display.Shutdown();

  EXPECT_NE(sink.str().find("\033[?25h"), std::string::npos);
}

TEST(TerminalDisplayTest, RenderSummaryShowsCumulativeStats) {
  std::ostringstream sink;
  const TerminalDisplay display(SampleContext(), sink);

  const std::string frame = display.RenderSummary(SampleResults());

  EXPECT_NE(frame.find("Complete"), std::string::npos);
  EXPECT_NE(frame.find("rand-read-4k"), std::string::npos);
  EXPECT_NE(frame.find("randread bs=4K iodepth=32"), std::string::npos);
  EXPECT_NE(frame.find("IOPS 118,200"), std::string::npos);
  EXPECT_NE(frame.find("483 MB/s"), std::string::npos);
  EXPECT_NE(frame.find("27.0 GiB"), std::string::npos);
  EXPECT_NE(frame.find("7,092,000 ops"), std::string::npos);
  EXPECT_NE(frame.find("min 1 p50 8 p95 32 p99 45 max 892"), std::string::npos);
  EXPECT_NE(frame.find("max 41,234"), std::string::npos);
  EXPECT_NE(frame.find("Err R:0 W:0"), std::string::npos);
  EXPECT_NE(frame.find("Log: ./cfio-results/run/cfio.log"), std::string::npos);
}

TEST(TerminalDisplayTest, RenderSummaryAggregatesTotals) {
  std::ostringstream sink;
  const TerminalDisplay display(SampleContext(), sink);

  const std::string frame = display.RenderSummary(SampleResults());

  EXPECT_NE(frame.find("TOTAL   IOPS 169,203   BW 681 MB/s   IO 39.0 GiB"), std::string::npos);
}

TEST(TerminalDisplayTest, ShowSummaryClearsAndWritesFrame) {
  std::ostringstream sink;
  TerminalDisplay display(SampleContext(), sink);

  display.ShowSummary(SampleResults());

  const std::string out = sink.str();
  EXPECT_NE(out.find("\033[2J"), std::string::npos);
  EXPECT_NE(out.find("\033[H"), std::string::npos);
  EXPECT_NE(out.find("rand-read-4k"), std::string::npos);
  EXPECT_NE(out.find("Complete"), std::string::npos);
}

TEST(DisplayFactoryTest, TerminalBackendCreatesDisplay) {
  auto display = DisplayFactory::Create("terminal", SampleContext());
  EXPECT_NE(display, nullptr);
}

}  // namespace
}  // namespace cfio
