/// @file display_context_build.cpp
/// @brief Implementation of the display header metadata builder

#include "display/display_context_build.h"

#include <filesystem>
#include <string>
#include <vector>

#include "common/cli_options.h"
#include "config/job_config.h"
#include "display/display_context.h"

namespace cfio {

std::string DeriveEngineLabel(const CliOptions& options, const std::vector<JobConfig>& jobs) {
  if (options.engine_override.has_value()) {
    return options.engine_override.value();
  }
  if (jobs.empty()) {
    return {};
  }
  const std::string& first = jobs.front().engine;
  for (const JobConfig& job : jobs) {
    if (job.engine != first) {
      return "mixed";
    }
  }
  return first;
}

std::string DeriveDirectLabel(const CliOptions& options, const std::vector<JobConfig>& jobs) {
  if (options.direct_override.has_value()) {
    return options.direct_override.value() ? "ON" : "OFF";
  }
  if (jobs.empty()) {
    return "OFF";
  }
  const bool first = jobs.front().direct;
  for (const JobConfig& job : jobs) {
    if (job.direct != first) {
      return "mixed";
    }
  }
  return first ? "ON" : "OFF";
}

DisplayContext MakeDisplayContext(const CliOptions& options, const std::vector<JobConfig>& jobs,
                                  const std::filesystem::path& log_path) {
  DisplayContext context;
  context.engine_label = DeriveEngineLabel(options, jobs);
  context.direct_label = DeriveDirectLabel(options, jobs);
  context.log_path = log_path;
  return context;
}

}  // namespace cfio
