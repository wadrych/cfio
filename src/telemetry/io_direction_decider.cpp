/// @file io_direction_decider.cpp
/// @brief Implementation of IODirectionDecider.

#include "telemetry/io_direction_decider.h"

namespace cfio {

IODirectionDecider::IODirectionDecider(RWMode mode, int rwmixread, std::uint64_t seed)
    : mode_(mode), rng_(seed), dist_(static_cast<double>(rwmixread) / 100.0) {
}

IODirection IODirectionDecider::Next() noexcept {
  switch (mode_) {
    case RWMode::kRead:
    case RWMode::kRandRead:
      return IODirection::kRead;
    case RWMode::kWrite:
    case RWMode::kRandWrite:
      return IODirection::kWrite;
    case RWMode::kReadWrite:
    case RWMode::kRandRW:
      return dist_(rng_) ? IODirection::kRead : IODirection::kWrite;
  }
  return IODirection::kRead;
}

}  // namespace cfio
