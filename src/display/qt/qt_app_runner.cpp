/// @file qt_app_runner.cpp
/// @brief Qt GUI front end. Preview feed until the orchestrator thread lands.

#include "display/qt/qt_app_runner.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <QApplication>
#include <QObject>
#include <QTimer>

#include "common/cli_options.h"
#include "config/job_config.h"
#include "display/display_context.h"
#include "display/qt/qt_main_window.h"
#include "display/qt/run_mailbox.h"
#include "logging/logger.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

constexpr int kPreviewSamples = 30;
constexpr int kPreviewIntervalMs = 1000;
constexpr std::uint64_t kPreviewPeakIops = 180000;
constexpr std::uint64_t kPreviewBlockSize = 4096;
constexpr std::uint64_t kPreviewBaseLatencyNs = 90000;
constexpr std::uint64_t kPreviewRamp = 10;

MetricsSnapshot EmptySnapshot(const std::vector<JobConfig>& jobs) {
  MetricsSnapshot snapshot;
  snapshot.jobs.reserve(jobs.size());
  for (const JobConfig& job : jobs) {
    PerJobMetrics metrics;
    metrics.job_name = job.name;
    snapshot.jobs.push_back(metrics);
  }
  snapshot.aggregate.job_name = "TOTAL";
  return snapshot;
}

std::vector<MetricsSnapshot> PreviewHistory(const std::vector<JobConfig>& jobs) {
  std::vector<MetricsSnapshot> history;
  history.reserve(kPreviewSamples);

  for (int second = 0; second < kPreviewSamples; ++second) {
    MetricsSnapshot snapshot = EmptySnapshot(jobs);
    snapshot.timestamp = std::chrono::steady_clock::time_point{} + std::chrono::seconds(second);

    const auto ramp = static_cast<std::uint64_t>(
        second < static_cast<int>(kPreviewRamp) ? second + 1 : kPreviewRamp);
    const std::uint64_t iops = kPreviewPeakIops * ramp / kPreviewRamp;
    snapshot.aggregate.iops_instant = iops;
    snapshot.aggregate.iops_cumulative = iops;
    snapshot.aggregate.bw_instant = iops * kPreviewBlockSize;
    snapshot.aggregate.bw_cumulative = iops * kPreviewBlockSize;
    snapshot.aggregate.lat_p50_ns = kPreviewBaseLatencyNs * ramp / kPreviewRamp;
    snapshot.aggregate.lat_p95_ns = snapshot.aggregate.lat_p50_ns * 2;
    snapshot.aggregate.lat_p99_ns = snapshot.aggregate.lat_p50_ns * 3;
    history.push_back(std::move(snapshot));
  }
  return history;
}

BenchmarkResults PreviewResults(const CliOptions& options, const std::vector<JobConfig>& jobs,
                                std::vector<MetricsSnapshot> history) {
  BenchmarkResults results;
  results.runtime_seconds = options.runtime_seconds;
  results.elapsed_seconds = kPreviewSamples;
  results.global_config = options;
  results.time_series = std::move(history);

  for (const JobConfig& job : jobs) {
    JobResults entry;
    entry.name = job.name;
    entry.config = job;
    entry.iops_avg = kPreviewPeakIops / jobs.size();
    entry.bw_avg_bytes = entry.iops_avg * kPreviewBlockSize;
    entry.total_bytes = entry.bw_avg_bytes * kPreviewSamples;
    entry.total_ios = entry.iops_avg * kPreviewSamples;
    results.jobs.push_back(std::move(entry));
  }
  return results;
}

DisplayContext PreviewContext(const CliOptions& options, const std::vector<JobConfig>& jobs) {
  DisplayContext context;
  context.engine_label =
      options.engine_override.value_or(jobs.empty() ? std::string{} : jobs.front().engine);
  const bool direct = options.direct_override.value_or(!jobs.empty() && jobs.front().direct);
  context.direct_label = direct ? "ON" : "OFF";
  context.log_path = "cfio.log";
  return context;
}

}  // namespace

int RunQtGui(int argc, char** argv, CliOptions opts, std::vector<JobConfig> jobs) {
  const CliOptions run_opts = std::move(opts);
  const std::vector<JobConfig> run_jobs = std::move(jobs);

  auto log = Logger::Get();
  log->info("qt gui: {} job(s), runtime {}s", run_jobs.size(), run_opts.runtime_seconds);

  int qt_argc = argc;
  const QApplication app(qt_argc, argv);

  RunMailbox mailbox;
  QtMainWindow window(mailbox, PreviewContext(run_opts, run_jobs));
  window.show();

  const std::vector<MetricsSnapshot> preview = PreviewHistory(run_jobs);
  mailbox.SetRuntime(kPreviewSamples);

  auto* feeder = new QTimer(&window);
  std::size_t next = 0;
  QObject::connect(feeder, &QTimer::timeout, &window,
                   [&mailbox, &preview, &next, feeder, &run_opts, &run_jobs]() {
                     if (next < preview.size() && !mailbox.StopRequested()) {
                       mailbox.PublishSnapshot(preview[next]);
                       ++next;
                       return;
                     }
                     feeder->stop();
                     std::vector<MetricsSnapshot> series(
                         preview.begin(), preview.begin() + static_cast<std::ptrdiff_t>(next));
                     BenchmarkResults results =
                         PreviewResults(run_opts, run_jobs, std::move(series));
                     results.interrupted = next < preview.size();
                     mailbox.PublishResults(results);
                   });
  feeder->start(kPreviewIntervalMs);

  return QApplication::exec();
}

}  // namespace cfio
