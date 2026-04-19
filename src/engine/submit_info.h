#ifndef CFIO_ENGINE_SUBMIT_INFO_H_
#define CFIO_ENGINE_SUBMIT_INFO_H_

/// @file submit_info.h
/// @brief Internal bookkeeping for async engine in-flight request correlation.

#include <chrono>

#include "common/types.h"

namespace cfio {
struct SubmitInfo {
  std::chrono::steady_clock::time_point submit_time;
  IODirection direction;
  size_t length;  ///< Requested byte count, used to detect short transfers
};

}  // namespace cfio

#endif  // CFIO_ENGINE_SUBMIT_INFO_H_
