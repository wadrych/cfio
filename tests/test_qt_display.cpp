/// @file test_qt_display.cpp
/// @brief Unit tests for QtDisplay

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "display/display_context.h"
#include "display/i_display.h"
#include "display/qt/qt_display.h"
#include "display/qt/run_mailbox.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

constexpr std::uint64_t kBytesPerIo = 4096;

DisplayContext MakeContext() {
  DisplayContext context;
  context.engine_label = "io_uring";
  context.direct_label = "O_DIRECT";
  context.log_path = "/tmp/cfio.log";
  return context;
}

MetricsSnapshot MakeSnapshot(std::uint64_t counter) {
  MetricsSnapshot snapshot;
  snapshot.timestamp = std::chrono::steady_clock::now();

  PerJobMetrics job;
  job.job_name = "job" + std::to_string(counter);
  job.iops_instant = counter;
  job.bw_instant = counter * kBytesPerIo;
  job.lat_p99_ns = counter * 100;
  snapshot.jobs.push_back(job);
  snapshot.aggregate = job;
  snapshot.aggregate.job_name = "TOTAL";

  return snapshot;
}

BenchmarkResults MakeResults() {
  BenchmarkResults results;
  results.runtime_seconds = 20;
  results.elapsed_seconds = 20.25;
  results.interrupted = false;

  JobResults job;
  job.name = "j1";
  job.total_ios = 4242;
  results.jobs.push_back(job);

  return results;
}

TEST(QtDisplayTest, InitSetsRuntimeAndPhase) {
  RunMailbox mailbox;
  QtDisplay display(mailbox, MakeContext());

  display.Init(30);

  EXPECT_EQ(mailbox.RuntimeSeconds(), 30);
  EXPECT_EQ(mailbox.CurrentPhase(), Phase::kRunning);
  EXPECT_EQ(mailbox.Sequence(), 0U);
}

TEST(QtDisplayTest, UpdatePublishesSnapshot) {
  RunMailbox mailbox;
  QtDisplay display(mailbox, MakeContext());

  display.Update(MakeSnapshot(7));
  EXPECT_EQ(mailbox.Sequence(), 1U);

  const MetricsSnapshot got = mailbox.LatestSnapshot();
  ASSERT_EQ(got.jobs.size(), 1U);
  EXPECT_EQ(got.jobs[0].job_name, "job7");
  EXPECT_EQ(got.jobs[0].iops_instant, 7U);
  EXPECT_EQ(got.jobs[0].bw_instant, 7U * kBytesPerIo);
  EXPECT_EQ(got.jobs[0].lat_p99_ns, 700U);
  EXPECT_EQ(got.aggregate.job_name, "TOTAL");

  display.Update(MakeSnapshot(8));
  EXPECT_EQ(mailbox.Sequence(), 2U);
  EXPECT_EQ(mailbox.LatestSnapshot().jobs[0].job_name, "job8");
}

TEST(QtDisplayTest, ShowSummaryPublishesResults) {
  RunMailbox mailbox;
  QtDisplay display(mailbox, MakeContext());

  display.Init(20);
  display.ShowSummary(MakeResults());

  EXPECT_EQ(mailbox.CurrentPhase(), Phase::kFinished);
  EXPECT_EQ(mailbox.Sequence(), 1U);

  const auto taken = mailbox.TakeResults();
  ASSERT_TRUE(taken.has_value());
  if (taken.has_value()) {
    ASSERT_EQ(taken->jobs.size(), 1U);
    EXPECT_EQ(taken->jobs[0].name, "j1");
    EXPECT_EQ(taken->jobs[0].total_ios, 4242U);
    EXPECT_DOUBLE_EQ(taken->elapsed_seconds, 20.25);
  }
}

TEST(QtDisplayTest, ShutdownMarksFinishedWithoutResults) {
  RunMailbox mailbox;
  QtDisplay display(mailbox, MakeContext());

  display.Init(5);
  display.Update(MakeSnapshot(1));
  ASSERT_EQ(mailbox.Sequence(), 1U);

  display.Shutdown();

  EXPECT_EQ(mailbox.CurrentPhase(), Phase::kFinished);
  EXPECT_EQ(mailbox.Sequence(), 1U);
  EXPECT_FALSE(mailbox.TakeResults().has_value());
}

TEST(QtDisplayTest, ShutdownAfterShowSummaryKeepsResults) {
  RunMailbox mailbox;
  QtDisplay display(mailbox, MakeContext());

  display.ShowSummary(MakeResults());
  display.Shutdown();

  EXPECT_EQ(mailbox.CurrentPhase(), Phase::kFinished);
  const auto taken = mailbox.TakeResults();
  ASSERT_TRUE(taken.has_value());
  if (taken.has_value()) {
    EXPECT_EQ(taken->runtime_seconds, 20);
  }
}

TEST(QtDisplayTest, StopRequestedForwardsMailboxFlag) {
  RunMailbox mailbox;
  const QtDisplay display(mailbox, MakeContext());

  EXPECT_FALSE(display.StopRequested());

  mailbox.RequestStop();
  EXPECT_TRUE(display.StopRequested());
}

TEST(QtDisplayTest, ContextIsRetained) {
  RunMailbox mailbox;
  const QtDisplay display(mailbox, MakeContext());

  EXPECT_EQ(display.Context().engine_label, "io_uring");
  EXPECT_EQ(display.Context().direct_label, "O_DIRECT");
  EXPECT_EQ(display.Context().log_path, "/tmp/cfio.log");
}

TEST(QtDisplayTest, FullLifecycleThroughInterface) {
  RunMailbox mailbox;
  QtDisplay display(mailbox, MakeContext());
  IDisplay& iface = display;

  EXPECT_FALSE(iface.StopRequested());

  iface.Init(10);
  EXPECT_EQ(mailbox.CurrentPhase(), Phase::kRunning);

  iface.Update(MakeSnapshot(1));
  iface.Update(MakeSnapshot(2));
  EXPECT_EQ(mailbox.Sequence(), 2U);

  mailbox.RequestStop();
  EXPECT_TRUE(iface.StopRequested());

  iface.ShowSummary(MakeResults());
  iface.Shutdown();

  EXPECT_EQ(mailbox.CurrentPhase(), Phase::kFinished);
  EXPECT_EQ(mailbox.Sequence(), 3U);
  EXPECT_TRUE(mailbox.TakeResults().has_value());
}

}  // namespace
}  // namespace cfio
