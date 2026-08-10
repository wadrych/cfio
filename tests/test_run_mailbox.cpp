/// @file test_run_mailbox.cpp
/// @brief Unit tests for RunMailbox

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "display/display_context.h"
#include "display/qt/run_mailbox.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

constexpr std::uint64_t kBytesPerIo = 512;

MetricsSnapshot MakeSnapshot(std::uint64_t counter) {
  MetricsSnapshot snapshot;
  snapshot.timestamp = std::chrono::steady_clock::now();

  PerJobMetrics job;
  job.job_name = "job" + std::to_string(counter);
  job.iops_instant = counter;
  job.bw_instant = counter * kBytesPerIo;
  snapshot.jobs.push_back(job);
  snapshot.aggregate = job;

  return snapshot;
}

BenchmarkResults MakeResults() {
  BenchmarkResults results;
  results.runtime_seconds = 30;
  results.elapsed_seconds = 30.5;
  results.interrupted = true;

  JobResults job;
  job.name = "j1";
  job.total_ios = 1000;
  results.jobs.push_back(job);

  return results;
}

TEST(RunMailboxTest, InitialState) {
  RunMailbox mailbox;

  EXPECT_EQ(mailbox.CurrentPhase(), Phase::kIdle);
  EXPECT_EQ(mailbox.Sequence(), 0U);
  EXPECT_EQ(mailbox.RuntimeSeconds(), 0);
  EXPECT_TRUE(mailbox.LatestSnapshot().jobs.empty());
  EXPECT_FALSE(mailbox.TakeResults().has_value());
  EXPECT_FALSE(mailbox.StopRequested());
}

TEST(RunMailboxTest, SetRuntimeMovesToRunning) {
  RunMailbox mailbox;
  mailbox.SetRuntime(60);

  EXPECT_EQ(mailbox.RuntimeSeconds(), 60);
  EXPECT_EQ(mailbox.CurrentPhase(), Phase::kRunning);
  EXPECT_EQ(mailbox.Sequence(), 0U);
}

TEST(RunMailboxTest, PublishSnapshotRoundTrip) {
  RunMailbox mailbox;

  MetricsSnapshot snapshot;
  PerJobMetrics first;
  first.job_name = "read";
  first.iops_instant = 125432;
  first.lat_p50_ns = 8000;
  first.lat_p95_ns = 32000;
  first.lat_p99_ns = 45000;
  PerJobMetrics second;
  second.job_name = "write";
  second.iops_instant = 64000;
  snapshot.jobs = {first, second};
  snapshot.aggregate.job_name = "TOTAL";
  snapshot.aggregate.iops_instant = 189432;

  mailbox.PublishSnapshot(snapshot);

  const MetricsSnapshot got = mailbox.LatestSnapshot();
  ASSERT_EQ(got.jobs.size(), 2U);
  EXPECT_EQ(got.jobs[0].job_name, "read");
  EXPECT_EQ(got.jobs[0].iops_instant, 125432U);
  EXPECT_EQ(got.jobs[0].lat_p50_ns, 8000U);
  EXPECT_EQ(got.jobs[0].lat_p95_ns, 32000U);
  EXPECT_EQ(got.jobs[0].lat_p99_ns, 45000U);
  EXPECT_EQ(got.jobs[1].job_name, "write");
  EXPECT_EQ(got.jobs[1].iops_instant, 64000U);
  EXPECT_EQ(got.aggregate.job_name, "TOTAL");
  EXPECT_EQ(got.aggregate.iops_instant, 189432U);
}

TEST(RunMailboxTest, SequenceIncrementsOnlyOnNewData) {
  RunMailbox mailbox;

  mailbox.SetRuntime(10);
  EXPECT_EQ(mailbox.Sequence(), 0U);

  mailbox.PublishSnapshot(MakeSnapshot(1));
  EXPECT_EQ(mailbox.Sequence(), 1U);

  mailbox.PublishSnapshot(MakeSnapshot(2));
  EXPECT_EQ(mailbox.Sequence(), 2U);

  EXPECT_FALSE(mailbox.LatestSnapshot().jobs.empty());
  EXPECT_EQ(mailbox.CurrentPhase(), Phase::kRunning);
  EXPECT_EQ(mailbox.RuntimeSeconds(), 10);
  EXPECT_EQ(mailbox.Sequence(), 2U);

  mailbox.PublishResults(MakeResults());
  EXPECT_EQ(mailbox.Sequence(), 3U);

  mailbox.MarkShutdown();
  EXPECT_EQ(mailbox.Sequence(), 3U);
}

TEST(RunMailboxTest, PublishResultsMovesToFinished) {
  RunMailbox mailbox;
  mailbox.SetRuntime(30);
  mailbox.PublishResults(MakeResults());

  EXPECT_EQ(mailbox.CurrentPhase(), Phase::kFinished);
  EXPECT_EQ(mailbox.Sequence(), 1U);
}

TEST(RunMailboxTest, TakeResultsConsumesOnce) {
  RunMailbox mailbox;
  mailbox.PublishResults(MakeResults());

  const auto taken = mailbox.TakeResults();
  ASSERT_TRUE(taken.has_value());
  if (taken.has_value()) {
    ASSERT_EQ(taken->jobs.size(), 1U);
    EXPECT_EQ(taken->jobs[0].name, "j1");
    EXPECT_TRUE(taken->interrupted);
    EXPECT_DOUBLE_EQ(taken->elapsed_seconds, 30.5);
  }

  EXPECT_FALSE(mailbox.TakeResults().has_value());
  EXPECT_EQ(mailbox.CurrentPhase(), Phase::kFinished);
}

TEST(RunMailboxTest, MarkShutdownWithoutResults) {
  RunMailbox mailbox;
  mailbox.SetRuntime(5);
  ASSERT_EQ(mailbox.CurrentPhase(), Phase::kRunning);

  mailbox.MarkShutdown();

  EXPECT_EQ(mailbox.CurrentPhase(), Phase::kFinished);
  EXPECT_FALSE(mailbox.TakeResults().has_value());
}

TEST(RunMailboxTest, RequestStopVisibleToWriter) {
  RunMailbox mailbox;
  ASSERT_FALSE(mailbox.StopRequested());

  mailbox.RequestStop();
  EXPECT_TRUE(mailbox.StopRequested());

  mailbox.RequestStop();
  EXPECT_TRUE(mailbox.StopRequested());
}

TEST(RunMailboxTest, ConcurrentPublishAndPoll) {
  constexpr std::uint64_t kPublishCount = 2000;

  RunMailbox mailbox;
  mailbox.SetRuntime(1);
  std::atomic<bool> writer_done{false};

  std::thread writer([&] {
    for (std::uint64_t i = 1; i <= kPublishCount; ++i) {
      if (mailbox.StopRequested()) {
        break;
      }
      mailbox.PublishSnapshot(MakeSnapshot(i));
    }
    writer_done.store(true);
  });

  std::uint64_t last_seq = 0;
  std::uint64_t observed = 0;
  while (!writer_done.load() || mailbox.Sequence() != last_seq) {
    const std::uint64_t seq = mailbox.Sequence();
    if (seq == last_seq) {
      continue;
    }
    ASSERT_GT(seq, last_seq);
    last_seq = seq;
    ++observed;

    const MetricsSnapshot got = mailbox.LatestSnapshot();
    ASSERT_EQ(got.jobs.size(), 1U);
    EXPECT_FALSE(got.jobs[0].job_name.empty());
    EXPECT_EQ(got.jobs[0].bw_instant, got.jobs[0].iops_instant * kBytesPerIo);
    EXPECT_EQ(got.aggregate.job_name, got.jobs[0].job_name);

    if (observed == 1) {
      mailbox.RequestStop();
    }
  }

  writer.join();

  EXPECT_TRUE(mailbox.StopRequested());
  EXPECT_GT(mailbox.Sequence(), 0U);
  EXPECT_LE(mailbox.Sequence(), kPublishCount);
}

TEST(RunMailboxTest, ContextIsTakenOnce) {
  RunMailbox mailbox;
  EXPECT_FALSE(mailbox.TakeContext().has_value());

  DisplayContext context;
  context.engine_label = "psync";
  context.direct_label = "ON";
  context.log_path = "cfio-results/job-20260810T120000/cfio.log";
  mailbox.PublishContext(context);

  const auto taken = mailbox.TakeContext();
  ASSERT_TRUE(taken.has_value());
  if (taken.has_value()) {
    EXPECT_EQ(taken->engine_label, "psync");
    EXPECT_EQ(taken->direct_label, "ON");
    EXPECT_EQ(taken->log_path, context.log_path);
  }

  EXPECT_FALSE(mailbox.TakeContext().has_value());
}

TEST(RunMailboxTest, PublishContextLeavesSequenceAlone) {
  RunMailbox mailbox;

  mailbox.PublishContext(DisplayContext{});
  EXPECT_EQ(mailbox.Sequence(), 0U);

  mailbox.PublishSnapshot(MakeSnapshot(1));
  EXPECT_EQ(mailbox.Sequence(), 1U);

  mailbox.PublishContext(DisplayContext{});
  EXPECT_EQ(mailbox.Sequence(), 1U);
}

}  // namespace
}  // namespace cfio
