#ifndef CFIO_DISPLAY_QT_QT_APP_RUNNER_H_
#define CFIO_DISPLAY_QT_QT_APP_RUNNER_H_

/// @file qt_app_runner.h
/// @brief Entry point of the Qt GUI front end

#include <vector>

#include "common/cli_options.h"
#include "config/job_config.h"

namespace cfio {

/// @brief Run the benchmark behind the Qt GUI
/// @param argc  Argument count, QApplication needs a reference that outlives it
/// @param argv  Argument vector
/// @param opts  Parsed and overridden command line options
/// @param jobs  Parsed and validated job configurations
/// @return Process exit code
int RunQtGui(int argc, char** argv, CliOptions opts, std::vector<JobConfig> jobs);

}  // namespace cfio

#endif  // CFIO_DISPLAY_QT_QT_APP_RUNNER_H_
