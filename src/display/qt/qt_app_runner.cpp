/// @file qt_app_runner.cpp
/// @brief Qt GUI front end running the benchmark on a worker thread

#include "display/qt/qt_app_runner.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <QApplication>

#include "common/cli_options.h"
#include "config/job_config.h"
#include "display/display_context.h"
#include "display/display_context_build.h"
#include "display/display_factory.h"
#include "display/i_display.h"
#include "display/qt/qt_display.h"
#include "display/qt/qt_main_window.h"
#include "display/qt/run_mailbox.h"
#include "logging/logger.h"
#include "orchestrator/benchmark_orchestrator.h"

namespace cfio {

int RunQtGui(int argc, char** argv, CliOptions opts, std::vector<JobConfig> jobs) {
  auto log = Logger::Get();
  log->info("qt gui: {} job(s), runtime {}s", jobs.size(), opts.runtime_seconds);

  int qt_argc = argc;
  const QApplication app(qt_argc, argv);

  RunMailbox mailbox;

  DisplayFactory::Register("qt", [&mailbox](const DisplayContext& context) {
    return std::unique_ptr<IDisplay>(std::make_unique<QtDisplay>(mailbox, context));
  });

  QtMainWindow window(mailbox, MakeDisplayContext(opts, jobs, {}));
  window.show();

  BenchmarkOrchestrator orchestrator(std::move(opts), std::move(jobs));
  int rc = EXIT_FAILURE;
  std::thread worker([&orchestrator, &mailbox, &rc]() {
    rc = orchestrator.Run();
    mailbox.MarkShutdown();
  });

  QApplication::exec();
  mailbox.RequestStop();
  worker.join();

  log->info("qt gui closed, exit code {}", rc);
  return rc;
}

}  // namespace cfio
