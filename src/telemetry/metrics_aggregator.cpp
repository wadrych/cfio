/// @file metrics_aggregator.cpp
/// @brief MetricsAggregator implementation

#include "telemetry/metrics_aggregator.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "telemetry/log2_histogram.h"

namespace cfio {
namespace {

constexpr auto kTickInterval = std::chrono::milliseconds(500);

constexpr std::uint64_t kTicksPerRecord = 2;

constexpr double kP50 = 0.50;
constexpr double kP95 = 0.95;
constexpr double kP99 = 0.99;

constexpr double kMinElapsed = 1e-9;

constexpr std::string_view kCfioVersion = "0.1.0";

/// @brief Fractional seconds between two steady clock points, floored.
double ElapsedSeconds(std::chrono::steady_clock::time_point begin,
                      std::chrono::steady_clock::time_point end) {
  const std::chrono::duration<double> span = end - begin;
  return std::max(span.count(), kMinElapsed);
}

/// @brief Round a non negative rate to an integer.
std::uint64_t RoundRate(double value) {
  if (value <= 0.0) {
    return 0;
  }
  return static_cast<std::uint64_t>(std::llround(value));
}

/// @brief Format a wall clock.
std::string FormatIso8601Utc(std::chrono::system_clock::time_point point) {
  const std::time_t secs = std::chrono::system_clock::to_time_t(point);
  std::tm utc{};
  if (gmtime_r(&secs, &utc) == nullptr) {
    return {};
  }
  std::array<char, 32> buffer{};
  const std::size_t written =
      std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return {buffer.data(), written};
}

}  // namespace

MetricsAggregator::MetricsAggregator(std::vector<WorkerThread*> workers, int runtime_seconds)
    : workers_(std::move(workers)),
      ts_capacity_(static_cast<std::size_t>(std::max(1, runtime_seconds))),
      time_series_(ts_capacity_),
      prev_iops_(workers_.size(), 0),
      prev_bytes_(workers_.size(), 0),
      runtime_seconds_(runtime_seconds) {
}

void MetricsAggregator::Start(const std::atomic<bool>& g_running) {
  start_time_ = std::chrono::steady_clock::now();
  prev_sample_time_ = start_time_;
  wall_start_time_ = std::chrono::system_clock::now();
  thread_ = std::jthread([this, &g_running](const std::stop_token& stop) { Run(stop, g_running); });
}

void MetricsAggregator::Stop() {
  if (thread_.joinable()) {
    thread_.request_stop();
    sample_cv_.notify_all();
    thread_.join();
  }
  end_time_ = std::chrono::steady_clock::now();
}

void MetricsAggregator::Run(const std::stop_token& stop, const std::atomic<bool>& g_running) {
  std::uint64_t tick = 0;

  while (g_running.load(std::memory_order_relaxed) && !stop.stop_requested()) {
    {
      std::unique_lock<std::mutex> lock(sample_mutex_);
      sample_cv_.wait_for(lock, stop, kTickInterval, [&stop]() { return stop.stop_requested(); });
    }
    if (stop.stop_requested() || !g_running.load(std::memory_order_relaxed)) {
      break;
    }

    ++tick;
    const bool record_ts = (tick % kTicksPerRecord == 0);
    const MetricsSnapshot snapshot = TakeSnapshot(record_ts);

    {
      const std::lock_guard<std::mutex> lock(snapshot_mutex_);
      latest_snapshot_ = snapshot;
    }
    if (record_ts) {
      RecordTimeSeries(snapshot);
    }
  }
}

MetricsSnapshot MetricsAggregator::TakeSnapshot(bool record_ts) {
  const auto now = std::chrono::steady_clock::now();
  const double since_start = ElapsedSeconds(start_time_, now);
  const double since_prev = ElapsedSeconds(prev_sample_time_, now);

  MetricsSnapshot snapshot;
  snapshot.timestamp = now;
  snapshot.jobs.reserve(workers_.size());

  Log2Histogram<64, std::uint64_t> global;
  PerJobMetrics aggregate;
  aggregate.job_name = "TOTAL";

  for (std::size_t i = 0; i < workers_.size(); ++i) {
    const WorkerThread& worker = *workers_[i];
    const std::uint64_t cur_iops = worker.IopsCount();
    const std::uint64_t cur_bytes = worker.BytesTransferred();
    const std::uint64_t delta_iops = cur_iops - prev_iops_[i];
    const std::uint64_t delta_bytes = cur_bytes - prev_bytes_[i];
    const Log2Histogram<64, std::uint64_t>& hist = worker.Histogram();

    PerJobMetrics metrics;
    metrics.job_name = worker.Config().name;
    metrics.iops_instant = RoundRate(static_cast<double>(delta_iops) / since_prev);
    metrics.bw_instant = RoundRate(static_cast<double>(delta_bytes) / since_prev);
    metrics.iops_cumulative = RoundRate(static_cast<double>(cur_iops) / since_start);
    metrics.bw_cumulative = RoundRate(static_cast<double>(cur_bytes) / since_start);
    metrics.lat_p50_ns = hist.Percentile(kP50);
    metrics.lat_p95_ns = hist.Percentile(kP95);
    metrics.lat_p99_ns = hist.Percentile(kP99);
    metrics.read_errors = worker.ReadErrorCount();
    metrics.write_errors = worker.WriteErrorCount();

    global.Merge(hist);
    aggregate.iops_instant += metrics.iops_instant;
    aggregate.iops_cumulative += metrics.iops_cumulative;
    aggregate.bw_instant += metrics.bw_instant;
    aggregate.bw_cumulative += metrics.bw_cumulative;
    aggregate.read_errors += metrics.read_errors;
    aggregate.write_errors += metrics.write_errors;

    snapshot.jobs.push_back(std::move(metrics));

    if (record_ts) {
      prev_iops_[i] = cur_iops;
      prev_bytes_[i] = cur_bytes;
    }
  }

  aggregate.lat_p50_ns = global.Percentile(kP50);
  aggregate.lat_p95_ns = global.Percentile(kP95);
  aggregate.lat_p99_ns = global.Percentile(kP99);
  snapshot.aggregate = std::move(aggregate);

  if (record_ts) {
    prev_sample_time_ = now;
  }
  return snapshot;
}

void MetricsAggregator::RecordTimeSeries(const MetricsSnapshot& snapshot) {
  time_series_[ts_write_index_] = snapshot;
  ts_write_index_ = (ts_write_index_ + 1) % ts_capacity_;
  ++ts_count_;
}

void MetricsAggregator::TakeFinalSnapshot() {
  MetricsSnapshot snapshot = TakeSnapshot(false);
  const std::lock_guard<std::mutex> lock(snapshot_mutex_);
  latest_snapshot_ = std::move(snapshot);
}

MetricsSnapshot MetricsAggregator::LatestSnapshot() const {
  const std::lock_guard<std::mutex> lock(snapshot_mutex_);
  return latest_snapshot_;
}

std::vector<MetricsSnapshot> MetricsAggregator::TimeSeries() const {
  std::vector<MetricsSnapshot> result;
  if (ts_count_ == 0) {
    return result;
  }

  if (ts_count_ <= ts_capacity_) {
    result.reserve(ts_count_);
    for (std::size_t i = 0; i < ts_count_; ++i) {
      result.push_back(time_series_[i]);
    }
    return result;
  }

  result.reserve(ts_capacity_);
  for (std::size_t k = 0; k < ts_capacity_; ++k) {
    const std::size_t idx = (ts_write_index_ + k) % ts_capacity_;
    result.push_back(time_series_[idx]);
  }
  return result;
}

BenchmarkResults MetricsAggregator::BuildResults(const CliOptions& opts) const {
  BenchmarkResults results;
  results.cfio_version = std::string(kCfioVersion);
  results.timestamp = FormatIso8601Utc(wall_start_time_);
  results.runtime_seconds = runtime_seconds_;
  results.global_config = opts;

  const auto end = end_time_ > start_time_ ? end_time_ : std::chrono::steady_clock::now();
  const double elapsed = ElapsedSeconds(start_time_, end);
  results.elapsed_seconds = elapsed;

  results.jobs.reserve(workers_.size());
  for (const WorkerThread* worker : workers_) {
    const std::uint64_t total_ios = worker->IopsCount();
    const std::uint64_t total_bytes = worker->BytesTransferred();
    const Log2Histogram<64, std::uint64_t>& hist = worker->Histogram();

    JobResults job;
    job.name = worker->Config().name;
    job.config = worker->Config();
    job.iops_avg = RoundRate(static_cast<double>(total_ios) / elapsed);
    job.bw_avg_bytes = RoundRate(static_cast<double>(total_bytes) / elapsed);
    job.lat_min_ns = hist.MinValue();
    job.lat_max_ns = hist.MaxValue();
    job.lat_p50_ns = hist.Percentile(kP50);
    job.lat_p95_ns = hist.Percentile(kP95);
    job.lat_p99_ns = hist.Percentile(kP99);
    job.total_ios = total_ios;
    job.total_bytes = total_bytes;
    job.read_errors = worker->ReadErrorCount();
    job.write_errors = worker->WriteErrorCount();
    job.direct_effective = worker->DirectEffective();
    results.jobs.push_back(std::move(job));
  }

  results.time_series = TimeSeries();
  return results;
}

}  // namespace cfio
