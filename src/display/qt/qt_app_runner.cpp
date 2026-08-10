/// @file qt_app_runner.cpp
/// @brief Qt GUI front end. Placeholder window until the widgets land.

#include "display/qt/qt_app_runner.h"

#include <string>
#include <utility>
#include <vector>

#include <QApplication>
#include <QMainWindow>
#include <QString>

#include "common/cli_options.h"
#include "config/job_config.h"
#include "display/qt/qt_job_table_widget.h"
#include "logging/logger.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

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

  auto* table = new QtJobTableWidget(&window);
  table->SetSnapshot(EmptySnapshot(run_jobs));
  window.setCentralWidget(table);

  window.show();

  return QApplication::exec();
}

}  // namespace cfio
