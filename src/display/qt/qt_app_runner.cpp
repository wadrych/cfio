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
#include "logging/logger.h"

namespace cfio {

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
  window.show();

  return QApplication::exec();
}

}  // namespace cfio
