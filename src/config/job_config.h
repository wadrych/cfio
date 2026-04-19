#ifndef CFIO_CONFIG_JOB_CONFIG_H_
#define CFIO_CONFIG_JOB_CONFIG_H_

/// @file job_config.h
/// @brief Per-job configuration parsed from JSON or CSV input.

#include <cstddef>
#include <filesystem>
#include <string>

#include "common/types.h"

namespace cfio {

/// Holds all configuration for a single benchmark job.
struct JobConfig {
  std::string name;
  std::string engine;
  RWMode rw_mode = RWMode::kRead;
  AccessPattern access_pattern = AccessPattern::kSequential;
  size_t block_size = 0;
  size_t file_size = 0;
  int iodepth = 1;
  bool direct = true;
  int rwmixread = 50;
  std::filesystem::path filename;
  size_t alignment = 4096;

  /// Parse a workload mode string to an enum.
  static RWMode ParseRWMode(const std::string& rw_str);

  /// Derive access pattern from the workload mode.
  static AccessPattern DeriveAccessPattern(RWMode mode);

  /// Convert RWMode enum to its string representation.
  static std::string ToString(RWMode mode);

  /// Convert AccessPattern enum to its string representation.
  static std::string ToString(AccessPattern pattern);

  /// Returns the IO queue depth of this job.
  [[nodiscard]] int EffectiveIODepth() const;
};

}  // namespace cfio

#endif  // CFIO_CONFIG_JOB_CONFIG_H_
