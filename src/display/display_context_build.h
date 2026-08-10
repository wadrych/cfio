#ifndef CFIO_DISPLAY_DISPLAY_CONTEXT_BUILD_H_
#define CFIO_DISPLAY_DISPLAY_CONTEXT_BUILD_H_

/// @file display_context_build.h
/// @brief Builds the run metadata shown in a display header

#include <filesystem>
#include <string>
#include <vector>

#include "common/cli_options.h"
#include "config/job_config.h"
#include "display/display_context.h"

namespace cfio {

/// @brief Label naming the engine used by the run
///
/// @param options  Parsed command line options, an engine override wins.
/// @param jobs     Job configurations, may be empty.
/// @return The engine name, "mixed" when jobs disagree, empty when there are no jobs.
std::string DeriveEngineLabel(const CliOptions& options, const std::vector<JobConfig>& jobs);

/// @brief Label describing whether O_DIRECT is active
///
/// @param options  Parsed command line options, a direct override wins.
/// @param jobs     Job configurations, may be empty.
/// @return "ON", "OFF", or "mixed" when jobs disagree.
std::string DeriveDirectLabel(const CliOptions& options, const std::vector<JobConfig>& jobs);

/// @brief Assemble the display header metadata
///
/// @param options   Parsed command line options.
/// @param jobs      Job configurations, may be empty.
/// @param log_path  Log file of this run, empty when it is not known yet.
/// @return Context holding the engine label, the direct label and the log path.
DisplayContext MakeDisplayContext(const CliOptions& options, const std::vector<JobConfig>& jobs,
                                  const std::filesystem::path& log_path);

}  // namespace cfio

#endif  // CFIO_DISPLAY_DISPLAY_CONTEXT_BUILD_H_
