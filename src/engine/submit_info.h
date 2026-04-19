#ifndef CFIO_ENGINE_SUBMIT_INFO_H_
#define CFIO_ENGINE_SUBMIT_INFO_H_

/// @file submit_info.h
/// @brief Internal bookkeeping for async engine request correlation.

#include <chrono>

#include "common/types.h"

namespace cfio {
struct SubmitInfo {
  std::chrono::steady_clock::time_point submit_time;  ///< When the IO was submitted
  IODirection direction;                              ///< Read or write
  size_t length;                                      ///< Requested byte count
};

}  // namespace cfio

#endif  // CFIO_ENGINE_SUBMIT_INFO_H_
