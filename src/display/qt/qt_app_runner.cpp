/// @file qt_app_runner.cpp
/// @brief Qt GUI front end. Placeholder window until the widgets land.

#include "display/qt/qt_app_runner.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <QApplication>
#include <QMainWindow>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "common/cli_options.h"
#include "config/job_config.h"
#include "display/qt/qt_chart_geometry.h"
#include "display/qt/qt_graph_widget.h"
#include "display/qt/qt_job_table_widget.h"
#include "logging/logger.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

constexpr int kPreviewSamples = 30;
constexpr std::uint64_t kPreviewPeakIops = 180000;
constexpr std::uint64_t kPreviewBlockSize = 4096;
constexpr std::uint64_t kPreviewBaseLatencyNs = 90000;
constexpr int kTableStretch = 2;
constexpr int kGraphStretch = 3;

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

    const auto ramp = static_cast<std::uint64_t>(second < 10 ? second + 1 : 10);
    const std::uint64_t iops = kPreviewPeakIops * ramp / 10;
    snapshot.aggregate.iops_instant = iops;
    snapshot.aggregate.iops_cumulative = iops;
    snapshot.aggregate.bw_instant = iops * kPreviewBlockSize;
    snapshot.aggregate.bw_cumulative = iops * kPreviewBlockSize;
    snapshot.aggregate.lat_p50_ns = kPreviewBaseLatencyNs * ramp / 10;
    snapshot.aggregate.lat_p95_ns = snapshot.aggregate.lat_p50_ns * 2;
    snapshot.aggregate.lat_p99_ns = snapshot.aggregate.lat_p50_ns * 3;
    history.push_back(std::move(snapshot));
  }
  return history;
}

}  // namespace

int RunQtGui(int argc, char** argv, CliOptions opts, std::vector<JobConfig> jobs) {
  const CliOptions run_opts = std::move(opts);
  const std::vector<JobConfig> run_jobs = std::move(jobs);

  auto log = Logger::Get();
  log->info("qt gui: {} job(s), runtime {}s", run_jobs.size(), run_opts.runtime_seconds);

  int qt_argc = argc;
  const QApplication app(qt_argc, argv);

  QMainWindow window;
  window.setWindowTitle(
      QString::fromStdString("C-FIO: " + std::to_string(run_jobs.size()) + " job(s)"));
  window.resize(1024, 720);

  const std::vector<MetricsSnapshot> preview = PreviewHistory(run_jobs);

  auto* central = new QWidget(&window);
  auto* layout = new QVBoxLayout(central);

  auto* table = new QtJobTableWidget(central);
  table->SetSnapshot(preview.back());
  layout->addWidget(table, kTableStretch);

  for (const ChartKind kind : {ChartKind::kIops, ChartKind::kBandwidth, ChartKind::kLatency}) {
    auto* graph = new QtGraphWidget(kind, central);
    graph->SetSeries(preview);
    layout->addWidget(graph, kGraphStretch);
  }

  window.setCentralWidget(central);

  window.show();

  return QApplication::exec();
}

}  // namespace cfio
