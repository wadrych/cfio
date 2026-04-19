#ifndef CFIO_ENGINE_SUBMIT_INFO_H_
#define CFIO_ENGINE_SUBMIT_INFO_H_

/// @file submit_info.h
/// @brief Internal bookkeeping for async engine in-flight request correlation.

#include <chrono>

#include "common/types.h"

namespace cfio {

/// Tracks submit-time metadata for in-flight async IO requests.
/// Used by LibaioEngine and IoUringEngine to correlate completions with
/// submissions via an unordered_map<uint64_t, SubmitInfo> keyed by request id.
struct SubmitInfo {
  std::chrono::steady_clock::time_point submit_time;   ///< From the original IORequest
  IODirection direction;                               ///< Read or write
};

}  // namespace cfio

#endif  // CFIO_ENGINE_SUBMIT_INFO_H_
