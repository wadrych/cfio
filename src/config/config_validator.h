#ifndef CFIO_CONFIG_CONFIG_VALIDATOR_H_
#define CFIO_CONFIG_CONFIG_VALIDATOR_H_

/// @file config_validator.h
/// @brief Validates JobConfig objects before benchmark execution.

#include <vector>

#include "config/job_config.h"

namespace cfio {

/// Enforces all validation rules on parsed job configurations.
class ConfigValidator {
 public:
  /// Validate job configuration.
  /// @param config The job to validate.
  /// @throws std::runtime_error if any rule is violated.
  static void Validate(const JobConfig& config);

  /// Validate all jobs.
  /// @param configs The full list of parsed jobs.
  /// @throws std::runtime_error if any rule is violated or the list is empty.
  static void ValidateAll(const std::vector<JobConfig>& configs);

 private:
  static void ValidateAlignment(const JobConfig& config);
  static void ValidateBlockSize(const JobConfig& config);
  static void ValidateFileSize(const JobConfig& config);
  static void ValidateEngine(const JobConfig& config);
  static void ValidateRWMode(const JobConfig& config);
  static void ValidateRWMixRead(const JobConfig& config);
  static void ValidateIODepth(const JobConfig& config);
  static void ValidateUniqueNames(const std::vector<JobConfig>& configs);
};

}  // namespace cfio

#endif  // CFIO_CONFIG_CONFIG_VALIDATOR_H_
