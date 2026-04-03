/// @file job_config.cpp
/// @brief Implementation of JobConfig helper methods.

#include "config/job_config.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace cfio {

RWMode JobConfig::ParseRWMode(const std::string& rw_str) {
  static const std::unordered_map<std::string, RWMode> kMap = {
      {"read", RWMode::kRead},
      {"write", RWMode::kWrite},
      {"randread", RWMode::kRandRead},
      {"randwrite", RWMode::kRandWrite},
      {"readwrite", RWMode::kReadWrite},
      {"randrw", RWMode::kRandRW},
  };

  auto it = kMap.find(rw_str);
  if (it == kMap.end()) {
    throw std::invalid_argument("unknown rw mode: '" + rw_str + "'");
  }
  return it->second;
}

AccessPattern JobConfig::DeriveAccessPattern(RWMode mode) {
  switch (mode) {
    case RWMode::kRead:
    case RWMode::kWrite:
    case RWMode::kReadWrite:
      return AccessPattern::kSequential;
    case RWMode::kRandRead:
    case RWMode::kRandWrite:
    case RWMode::kRandRW:
      return AccessPattern::kRandom;
  }
  // Unreachable if all enum values are handled. 
  return AccessPattern::kSequential;
}

std::string JobConfig::ToString(RWMode mode) {
  switch (mode) {
    case RWMode::kRead:      return "read";
    case RWMode::kWrite:     return "write";
    case RWMode::kRandRead:  return "randread";
    case RWMode::kRandWrite: return "randwrite";
    case RWMode::kReadWrite: return "readwrite";
    case RWMode::kRandRW:    return "randrw";
  }
  return "unknown";
}

std::string JobConfig::ToString(AccessPattern pattern) {
  switch (pattern) {
    case AccessPattern::kSequential: return "sequential";
    case AccessPattern::kRandom:     return "random";
  }
  return "unknown";
}

int JobConfig::EffectiveIODepth() const {
  // Sequential engines cannot have iodepth greater than 1. 
  if (engine == "sync" || engine == "psync") {
    return 1;
  }
  return iodepth;
}

}  // namespace cfio
