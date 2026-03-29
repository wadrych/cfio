/// @file config_validator.cpp
/// @brief Implementation of ConfigValidator rules.

#include "config/config_validator.h"

#include <stdexcept>
#include <string>
#include <unordered_set>

namespace cfio {

void ConfigValidator::Validate(const JobConfig& config) {
  if (config.name.empty()) {
    throw std::runtime_error("job name must not be empty");
  }

  ValidateAlignment(config);
  ValidateBlockSize(config);
  ValidateFileSize(config);
  ValidateEngine(config);
  ValidateRWMode(config);
  ValidateRWMixRead(config);
  ValidateIODepth(config);
}

void ConfigValidator::ValidateAll(const std::vector<JobConfig>& configs) {
  if (configs.empty()) {
    throw std::runtime_error("job list must not be empty");
  }

  for (const auto& config : configs) {
    Validate(config);
  }

  ValidateUniqueNames(configs);
}

void ConfigValidator::ValidateAlignment(const JobConfig& config) {
  if (config.alignment == 0 ||
      (config.alignment & (config.alignment - 1)) != 0) {
    throw std::runtime_error("job '" + config.name + "': alignment (" +
                             std::to_string(config.alignment) +
                             ") must be a positive power of 2");
  }
}

void ConfigValidator::ValidateBlockSize(const JobConfig& config) {
  if (config.block_size == 0) {
    throw std::runtime_error("job '" + config.name +
                             "': block_size must be greater than 0");
  }
  if (config.block_size % config.alignment != 0) {
    throw std::runtime_error(
        "job '" + config.name + "': block_size (" +
        std::to_string(config.block_size) +
        ") must be a positive multiple of alignment (" +
        std::to_string(config.alignment) + ")");
  }
}

void ConfigValidator::ValidateFileSize(const JobConfig& config) {
  if (config.file_size < config.block_size) {
    throw std::runtime_error(
        "job '" + config.name + "': file_size (" +
        std::to_string(config.file_size) +
        ") must be >= block_size (" +
        std::to_string(config.block_size) + ")");
  }
  if (config.file_size % config.alignment != 0) {
    throw std::runtime_error(
        "job '" + config.name + "': file_size (" +
        std::to_string(config.file_size) +
        ") must be a multiple of alignment (" +
        std::to_string(config.alignment) + ")");
  }
}

void ConfigValidator::ValidateEngine(const JobConfig& config) {
  // to-do, when Engines are created, please replace this with a more robust mechanism
  static const std::unordered_set<std::string> kKnownEngines = {
      "psync", "sync", "libaio", "io_uring"};

  if (kKnownEngines.find(config.engine) == kKnownEngines.end()) {
    throw std::runtime_error("job '" + config.name + "': unknown engine '" +
                             config.engine + "'");
  }
}

void ConfigValidator::ValidateRWMode(const JobConfig& config) {
  switch (config.rw_mode) {
    case RWMode::kRead:
    case RWMode::kWrite:
    case RWMode::kRandRead:
    case RWMode::kRandWrite:
    case RWMode::kReadWrite:
    case RWMode::kRandRW:
      return;
  }
  throw std::runtime_error("job '" + config.name +
                           "': invalid rw_mode enum value (" +
                           std::to_string(static_cast<int>(config.rw_mode)) +
                           ")");
}

void ConfigValidator::ValidateRWMixRead(const JobConfig& config) {
  if (config.rwmixread < 0 || config.rwmixread > 100) {
    throw std::runtime_error("job '" + config.name + "': rwmixread (" +
                             std::to_string(config.rwmixread) +
                             ") must be in range 0-100");
  }
}

void ConfigValidator::ValidateIODepth(const JobConfig& config) {
  if (config.iodepth < 1) {
    throw std::runtime_error("job '" + config.name + "': iodepth (" +
                             std::to_string(config.iodepth) +
                             ") must be >= 1");
  }
}

void ConfigValidator::ValidateUniqueNames(
    const std::vector<JobConfig>& configs) {
  std::unordered_set<std::string> seen;
  for (const auto& config : configs) {
    if (!seen.insert(config.name).second) {
      throw std::runtime_error("duplicate job name: '" + config.name + "'");
    }
  }
}

}  // namespace cfio
