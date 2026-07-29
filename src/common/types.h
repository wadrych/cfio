#ifndef CFIO_COMMON_TYPES_H_
#define CFIO_COMMON_TYPES_H_

/// @file types.h
/// @brief Common enums and structures shared across all modules.

#include <sys/types.h>

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace cfio {

/// @brief Identifies whether a single IO operation is a read or write.
enum class IODirection {
  kRead,  ///< Read from the file
  kWrite  ///< Write to the file
};

/// @brief Describes the access pattern.
enum class AccessPattern {
  kSequential,  ///< Offsets advance in order
  kRandom       ///< Offsets are drawn at random
};

/// @brief Workload mode requested in the job configuration.
enum class RWMode {
  kRead,       ///< Sequential read only
  kWrite,      ///< Sequential write only
  kRandRead,   ///< Random read only
  kRandWrite,  ///< Random write only
  kReadWrite,  ///< Sequential mix of reads and writes
  kRandRW,     ///< Random mix of reads and writes
};

/// @brief Describes a single IO operation submitted from a worker thread to an engine.
struct IORequest {
  off_t offset{};                                     ///< Aligned file offset
  void* buffer{};                                     ///< Aligned IO buffer
  size_t length{};                                    ///< IO size in bytes
  IODirection direction{};                            ///< Read or write
  std::chrono::steady_clock::time_point submit_time;  ///< Set just before submission
  uint64_t id{};                                      ///< Per-worker monotonic counter
};

/// @brief Result of a completed IO operation.
struct IOCompletion {
  uint64_t id{};                ///< Matches IORequest::id
  ssize_t bytes_transferred{};  ///< Actual bytes read or written
  IODirection direction{};      ///< Read or write, carried from IORequest
  bool success{};               ///< True if IO completed without error
  int error_code{};             ///< errno on failure, EIO on short transfer, zero on success
  std::chrono::steady_clock::time_point submit_time;  ///< Carried from IORequest
};

}  // namespace cfio

#endif  // CFIO_COMMON_TYPES_H_
