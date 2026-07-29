/// @file test_metrics_aggregator.cpp
/// @brief Unit tests for MetricsAggregator

#include <algorithm>
#include <atomic>
#include <barrier>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "common/cli_options.h"
#include "common/types.h"
#include "config/job_config.h"
#include "engine/i_engine_io.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_aggregator.h"
#include "telemetry/metrics_snapshot.h"
#include "telemetry/worker_thread.h"

namespace cfio {

class MetricsAggregatorTestPeer {
 public:
  explicit MetricsAggregatorTestPeer(MetricsAggregator& agg) : agg_(&agg) {}

  void SetPrev(std::vector<std::uint64_t> iops, std::vector<std::uint64_t> bytes) {
    agg_->prev_iops_ = std::move(iops);
    agg_->prev_bytes_ = std::move(bytes);
  }

  void SetClocks(std::chrono::steady_clock::time_point start,
                 std::chrono::steady_clock::time_point prev) {
    agg_->start_time_ = start;
    agg_->prev_sample_time_ = prev;
  }

  void SetEndTime(std::chrono::steady_clock::time_point end) { agg_->end_time_ = end; }

  MetricsSnapshot TakeSnapshot(bool record_ts) { return agg_->TakeSnapshot(record_ts); }

  void RecordTimeSeries(const MetricsSnapshot& snapshot) { agg_->RecordTimeSeries(snapshot); }

  [[nodiscard]] const std::vector<std::uint64_t>& PrevIops() const { return agg_->prev_iops_; }

 private:
  MetricsAggregator* agg_;
};

namespace {

constexpr std::size_t kBlockSize = 4096;
constexpr std::size_t kFileSize = 65536;

class CountingEngine : public IEngineIO {
 public:
  std::uint64_t stop_after = 0;
  bool fail_all = false;
  bool direct_enabled = false;
  std::atomic<bool>* running = nullptr;

  void Open(const JobConfig& /*config*/) override {}

  void SubmitIO(const IORequest& request) override {
    pending_.push_back(Pending{request.submit_time, request.direction, request.length});
  }

  void PollCompletions(int min_events, int max_events, std::vector<IOCompletion>& out) override {
    if (pending_.empty()) {
      return;
    }
    const int available = static_cast<int>(pending_.size());
    int count = std::min(max_events, available);
    if (count < min_events) {
      count = std::min(min_events, available);
    }

    for (int i = 0; i < count; ++i) {
      const Pending info = pending_.front();
      pending_.pop_front();
      ++completed_;

      IOCompletion completion{};
      completion.direction = info.direction;
      completion.submit_time = info.submit_time;
      if (fail_all) {
        completion.bytes_transferred = 0;
        completion.success = false;
        completion.error_code = EIO;
      } else {
        completion.bytes_transferred = static_cast<ssize_t>(info.length);
        completion.success = true;
        completion.error_code = 0;
      }
      out.push_back(completion);

      if (stop_after != 0 && completed_ >= stop_after && running != nullptr) {
        running->store(false, std::memory_order_relaxed);
      }
    }
  }

  void Close() override {}

  [[nodiscard]] bool IsDirectEnabled() const noexcept override { return direct_enabled; }

 private:
  struct Pending {
    std::chrono::steady_clock::time_point submit_time;
    IODirection direction;
    std::size_t length;
  };

  std::deque<Pending> pending_;
  std::uint64_t completed_ = 0;
};

// Job for sync worker
JobConfig MakeConfig(const std::string& name, RWMode rw_mode, bool direct) {
  JobConfig cfg;
  cfg.name = name;
  cfg.engine = "psync";
  cfg.rw_mode = rw_mode;
  cfg.access_pattern = JobConfig::DeriveAccessPattern(rw_mode);
  cfg.block_size = kBlockSize;
  cfg.file_size = kFileSize;
  cfg.iodepth = 1;
  cfg.direct = direct;
  cfg.rwmixread = 50;
  cfg.alignment = kBlockSize;
  return cfg;
}

std::unique_ptr<WorkerThread> MakeFrozenWorker(const std::string& name, RWMode rw_mode,
                                               std::uint64_t target, bool fail_all = false,
                                               bool direct = false) {
  auto engine = std::make_unique<CountingEngine>();
  engine->stop_after = target;
  engine->fail_all = fail_all;
  engine->direct_enabled = direct;

  std::atomic<bool> running{true};
  engine->running = &running;

  auto worker =
      std::make_unique<WorkerThread>(MakeConfig(name, rw_mode, direct), std::move(engine));
  std::barrier<> start_barrier{1};
  worker->Start(start_barrier, running);
  worker->Join();
  return worker;
}

MetricsSnapshot TaggedSnapshot(std::uint64_t tag) {
  MetricsSnapshot snapshot;
  snapshot.aggregate.iops_cumulative = tag;
  return snapshot;
}

TEST(MetricsAggregatorTest, SnapshotMergeAggregatesJobs) {
  constexpr std::uint64_t kReadIos = 50;
  constexpr std::uint64_t kWriteIos = 100;
  auto reader = MakeFrozenWorker("reader", RWMode::kRead, kReadIos);
  auto writer = MakeFrozenWorker("writer", RWMode::kWrite, kWriteIos);

  MetricsAggregator aggregator({reader.get(), writer.get()}, 10);
  MetricsAggregatorTestPeer peer(aggregator);
  const auto now = std::chrono::steady_clock::now();
  peer.SetClocks(now - std::chrono::seconds(2), now - std::chrono::seconds(1));

  const MetricsSnapshot snapshot = peer.TakeSnapshot(true);

  ASSERT_EQ(snapshot.jobs.size(), 2U);
  EXPECT_EQ(snapshot.jobs[0].job_name, "reader");
  EXPECT_EQ(snapshot.jobs[1].job_name, "writer");

  const PerJobMetrics& agg = snapshot.aggregate;
  EXPECT_EQ(agg.iops_instant, snapshot.jobs[0].iops_instant + snapshot.jobs[1].iops_instant);
  EXPECT_EQ(agg.iops_cumulative,
            snapshot.jobs[0].iops_cumulative + snapshot.jobs[1].iops_cumulative);
  EXPECT_EQ(agg.bw_instant, snapshot.jobs[0].bw_instant + snapshot.jobs[1].bw_instant);
  EXPECT_EQ(agg.bw_cumulative, snapshot.jobs[0].bw_cumulative + snapshot.jobs[1].bw_cumulative);
  EXPECT_EQ(agg.read_errors, snapshot.jobs[0].read_errors + snapshot.jobs[1].read_errors);
  EXPECT_EQ(agg.write_errors, snapshot.jobs[0].write_errors + snapshot.jobs[1].write_errors);
  EXPECT_GT(agg.iops_cumulative, 0U);
}

TEST(MetricsAggregatorTest, InstantVsCumulativeMath) {
  constexpr std::uint64_t kIos = 100;
  constexpr std::uint64_t kPrevIos = 20;
  auto worker = MakeFrozenWorker("job", RWMode::kRead, kIos);

  MetricsAggregator aggregator({worker.get()}, 10);
  MetricsAggregatorTestPeer peer(aggregator);
  const auto now = std::chrono::steady_clock::now();
  peer.SetClocks(now - std::chrono::seconds(2), now - std::chrono::seconds(1));
  peer.SetPrev({kPrevIos}, {kPrevIos * kBlockSize});

  const MetricsSnapshot first = peer.TakeSnapshot(true);
  EXPECT_EQ(first.aggregate.iops_instant, (kIos - kPrevIos) / 1);  // 80
  EXPECT_EQ(first.aggregate.iops_cumulative, kIos / 2);            // 50
  EXPECT_EQ(first.aggregate.bw_instant, (kIos - kPrevIos) * kBlockSize);
  EXPECT_EQ(first.aggregate.bw_cumulative, (kIos / 2) * kBlockSize);
  ASSERT_EQ(peer.PrevIops().size(), 1U);
  EXPECT_EQ(peer.PrevIops()[0], kIos);

  const MetricsSnapshot second = peer.TakeSnapshot(false);
  EXPECT_EQ(second.aggregate.iops_instant, 0U);
  EXPECT_GT(second.aggregate.iops_cumulative, 0U);
}

TEST(MetricsAggregatorTest, CircularBufferWraps) {
  MetricsAggregator aggregator({}, 3);
  MetricsAggregatorTestPeer peer(aggregator);

  for (std::uint64_t tag = 1; tag <= 5; ++tag) {
    peer.RecordTimeSeries(TaggedSnapshot(tag));
  }

  const std::vector<MetricsSnapshot> series = aggregator.TimeSeries();
  ASSERT_EQ(series.size(), 3U);
  EXPECT_EQ(series[0].aggregate.iops_cumulative, 3U);
  EXPECT_EQ(series[1].aggregate.iops_cumulative, 4U);
  EXPECT_EQ(series[2].aggregate.iops_cumulative, 5U);
}

TEST(MetricsAggregatorTest, TimeSeriesBeforeWrap) {
  MetricsAggregator aggregator({}, 3);
  MetricsAggregatorTestPeer peer(aggregator);

  peer.RecordTimeSeries(TaggedSnapshot(1));
  peer.RecordTimeSeries(TaggedSnapshot(2));

  const std::vector<MetricsSnapshot> series = aggregator.TimeSeries();
  ASSERT_EQ(series.size(), 2U);
  EXPECT_EQ(series[0].aggregate.iops_cumulative, 1U);
  EXPECT_EQ(series[1].aggregate.iops_cumulative, 2U);
}

TEST(MetricsAggregatorTest, BuildResultsCarriesWorkerTotals) {
  constexpr std::uint64_t kReadIos = 50;
  constexpr std::uint64_t kWriteIos = 100;
  auto reader = MakeFrozenWorker("reader", RWMode::kRead, kReadIos, false, true);
  auto writer = MakeFrozenWorker("writer", RWMode::kWrite, kWriteIos, false, false);

  MetricsAggregator aggregator({reader.get(), writer.get()}, 7);
  MetricsAggregatorTestPeer peer(aggregator);
  const auto now = std::chrono::steady_clock::now();
  peer.SetClocks(now - std::chrono::seconds(1), now - std::chrono::seconds(1));

  CliOptions opts;
  opts.ui_backend = "terminal";
  opts.runtime_seconds = 7;
  const BenchmarkResults results = aggregator.BuildResults(opts);

  EXPECT_EQ(results.cfio_version, "0.1.0");
  EXPECT_EQ(results.runtime_seconds, 7);
  EXPECT_FALSE(results.interrupted);
  EXPECT_EQ(results.global_config.ui_backend, "terminal");
  EXPECT_TRUE(results.time_series.empty());
  ASSERT_EQ(results.jobs.size(), 2U);

  const JobResults& read_job = results.jobs[0];
  EXPECT_EQ(read_job.name, "reader");
  EXPECT_EQ(read_job.total_ios, kReadIos);
  EXPECT_EQ(read_job.total_bytes, kReadIos * kBlockSize);
  EXPECT_EQ(read_job.read_errors, 0U);
  EXPECT_EQ(read_job.write_errors, 0U);
  EXPECT_TRUE(read_job.direct_effective);
  EXPECT_GT(read_job.iops_avg, 0U);
  EXPECT_GT(read_job.bw_avg_bytes, 0U);
  EXPECT_LE(read_job.lat_min_ns, read_job.lat_p50_ns);
  EXPECT_LE(read_job.lat_p50_ns, read_job.lat_p95_ns);
  EXPECT_LE(read_job.lat_p95_ns, read_job.lat_p99_ns);
  EXPECT_LE(read_job.lat_p99_ns, read_job.lat_max_ns);

  const JobResults& write_job = results.jobs[1];
  EXPECT_EQ(write_job.name, "writer");
  EXPECT_EQ(write_job.total_ios, kWriteIos);
  EXPECT_EQ(write_job.total_bytes, kWriteIos * kBlockSize);
  EXPECT_FALSE(write_job.direct_effective);
}

TEST(MetricsAggregatorTest, ElapsedSecondsMeasuredIndependentOfConfiguredRuntime) {
  constexpr std::uint64_t kIos = 500;
  auto reader = MakeFrozenWorker("reader", RWMode::kRead, kIos, false, true);

  MetricsAggregator aggregator({reader.get()}, 30);
  MetricsAggregatorTestPeer peer(aggregator);
  const auto now = std::chrono::steady_clock::now();
  peer.SetClocks(now - std::chrono::seconds(4), now - std::chrono::seconds(4));
  peer.SetEndTime(now);

  CliOptions opts;
  opts.runtime_seconds = 30;
  const BenchmarkResults results = aggregator.BuildResults(opts);

  EXPECT_EQ(results.runtime_seconds, 30);
  EXPECT_NEAR(results.elapsed_seconds, 4.0, 0.05);
}

TEST(MetricsAggregatorTest, ReadWriteErrorsSplitInResults) {
  constexpr std::uint64_t kErrors = 30;
  auto reader = MakeFrozenWorker("reader", RWMode::kRead, kErrors, true);
  auto writer = MakeFrozenWorker("writer", RWMode::kWrite, kErrors, true);

  MetricsAggregator aggregator({reader.get(), writer.get()}, 5);
  MetricsAggregatorTestPeer peer(aggregator);
  const auto now = std::chrono::steady_clock::now();
  peer.SetClocks(now - std::chrono::seconds(1), now - std::chrono::seconds(1));

  const MetricsSnapshot snapshot = peer.TakeSnapshot(false);
  EXPECT_EQ(snapshot.jobs[0].read_errors, kErrors);
  EXPECT_EQ(snapshot.jobs[0].write_errors, 0U);
  EXPECT_EQ(snapshot.jobs[1].write_errors, kErrors);
  EXPECT_EQ(snapshot.jobs[1].read_errors, 0U);
  EXPECT_EQ(snapshot.aggregate.read_errors, kErrors);
  EXPECT_EQ(snapshot.aggregate.write_errors, kErrors);

  const BenchmarkResults results = aggregator.BuildResults(CliOptions{});
  EXPECT_EQ(results.jobs[0].read_errors, kErrors);
  EXPECT_EQ(results.jobs[0].write_errors, 0U);
  EXPECT_EQ(results.jobs[1].write_errors, kErrors);
  EXPECT_EQ(results.jobs[1].read_errors, 0U);
}

TEST(MetricsAggregatorTest, StartStopLifecycleRecordsSeries) {
  auto worker = MakeFrozenWorker("job", RWMode::kRead, 40);

  MetricsAggregator aggregator({worker.get()}, 5);
  const std::atomic<bool> g_running{true};
  aggregator.Start(g_running);
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));
  aggregator.Stop();
  aggregator.TakeFinalSnapshot();

  EXPECT_EQ(aggregator.LatestSnapshot().jobs.size(), 1U);
  EXPECT_GE(aggregator.TimeSeries().size(), 1U);
}

}  // namespace
}  // namespace cfio
