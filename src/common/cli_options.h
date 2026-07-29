#ifndef CFIO_COMMON_CLI_OPTIONS_H_
#define CFIO_COMMON_CLI_OPTIONS_H_

/// @file cli_options.h
/// @brief Parsed CLI arguments for the cfio application.

#include <filesystem>
#include <optional>
#include <string>

namespace cfio {

/// @brief Parsed command-line arguments controlling runtime behavior.
struct CliOptions {
  /// Path to the job definition file in json or csv format.
  std::filesystem::path config_path;

  /// Global benchmark duration in seconds.
  int runtime_seconds = 60;

  /// Results output directory. Empty auto-generates a timestamped path under
  /// ./cfio-results named after the first job.
  std::filesystem::path output_dir;

  /// UI backend selection: "terminal", "tui", or "qt".
  std::string ui_backend = "terminal";

  /// O_DIRECT override for all jobs.
  std::optional<bool> direct_override;

  /// Override IO engine for all jobs.
  std::optional<std::string> engine_override;

  /// Enable Debug level logging.
  bool verbose = false;

  /// Keep test files after the benchmark completes instead of deleting them.
  bool keep_files = false;
};

}  // namespace cfio

#endif  // CFIO_COMMON_CLI_OPTIONS_H_
