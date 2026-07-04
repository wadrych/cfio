#ifndef CFIO_COMMON_TYPES_H_
#define CFIO_COMMON_TYPES_H_

/// @file types.h
/// @brief Common enums and structures shared across all modules.

#include <sys/types.h>

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace cfio {

/// Identifies whether a single IO operation is a read or write.
enum class IODirection { kRead, kWrite };

/// Describes the access pattern.
enum class AccessPattern { kSequential, kRandom };

/// Workload mode.
enum class RWMode {
  kRead,
  kWrite,
  kRandRead,
  kRandWrite,
  kReadWrite,
  kRandRW,
};

/// Describes a single IO operation submitted from a worker thread to an engine.
struct IORequest {
  off_t offset{};                                     ///< File offset (aligned)
  void* buffer{};                                     ///< Aligned IO buffer
  size_t length{};                                    ///< IO size in bytes
  IODirection direction{};                            ///< Read or write
  std::chrono::steady_clock::time_point submit_time;  ///< Set just before submitIO()
  uint64_t id{};                                      ///< Per-worker monotonic counter
};

/// Result of a completed IO operation.
struct IOCompletion {
  uint64_t id{};                ///< Matches IORequest::id
  ssize_t bytes_transferred{};  ///< Actual bytes read/written
  IODirection direction{};      ///< Read or write (carried from IORequest)
  bool success{};               ///< True if IO completed without error
  int error_code{};             ///< errno on failure, EIO on short transfer, 0 on success
  std::chrono::steady_clock::time_point submit_time;  ///< Carried from IORequest
};

}  // namespace cfio

#endif  // CFIO_COMMON_TYPES_H_
