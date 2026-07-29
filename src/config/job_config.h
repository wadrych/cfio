#ifndef CFIO_CONFIG_JOB_CONFIG_H_
#define CFIO_CONFIG_JOB_CONFIG_H_

/// @file job_config.h
/// @brief Per-job configuration parsed from JSON or CSV input.

#include <cstddef>
#include <filesystem>
#include <string>

#include "common/types.h"

namespace cfio {

/// @brief Holds all configuration for a single benchmark job.
struct JobConfig {
  std::string name;                ///< Unique job name used in reports
  std::string engine;              ///< IO engine name
  RWMode rw_mode = RWMode::kRead;  ///< Workload mode requested
  AccessPattern access_pattern = AccessPattern::kSequential;  ///< Derived from rw_mode
  size_t block_size = 0;                                      ///< IO size in bytes per operation
  size_t file_size = 0;                                       ///< Test file size in bytes
  int iodepth = 1;                 ///< Requested queue depth, ignored by sync engines
  bool direct = true;              ///< Request O_DIRECT for this job
  int rwmixread = 50;              ///< Read percentage for mixed workloads
  std::filesystem::path filename;  ///< Path of the file used for IO
  size_t alignment = 4096;         ///< Buffer and offset alignment in bytes

  /// @brief Parse a workload mode string to an enum.
  /// @param rw_str  Mode name such as read, randwrite or randrw.
  /// @return The matching RWMode value.
  /// @throws std::invalid_argument if the string is not a known mode.
  static RWMode ParseRWMode(const std::string& rw_str);

  /// @brief Derive the access pattern from the workload mode.
  /// @param mode  Workload mode to inspect.
  /// @return kRandom for the rand modes, kSequential otherwise.
  static AccessPattern DeriveAccessPattern(RWMode mode);

  /// @brief Convert an RWMode enum to its string representation.
  /// @param mode  Workload mode to convert.
  /// @return The mode name, or "unknown" if the value is out of range.
  static std::string ToString(RWMode mode);

  /// @brief Convert an AccessPattern enum to its string representation.
  /// @param pattern  Access pattern to convert.
  /// @return The pattern name, or "unknown" if the value is out of range.
  static std::string ToString(AccessPattern pattern);

  /// @brief Report the queue depth this job actually uses.
  /// @return One for synchronous engines, the configured iodepth otherwise.
  [[nodiscard]] int EffectiveIODepth() const;
};

}  // namespace cfio

#endif  // CFIO_CONFIG_JOB_CONFIG_H_
