#ifndef CFIO_TELEMETRY_OFFSET_GENERATOR_H_
#define CFIO_TELEMETRY_OFFSET_GENERATOR_H_

/// @file offset_generator.h
/// @brief Produces the next aligned file offset for a worker IO.

#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <random>

#include "common/types.h"

namespace cfio {

/// @brief Generates aligned file offsets for one worker, either walking the file
///        sequentially or picking random positions.
///
/// A worker owns a single instance and calls Next() once per IO.
class OffsetGenerator {
 public:
  /// @brief Builds a generator for one job.
  /// @param pattern    Sequential or random access.
  /// @param file_size  Size of the target file.
  /// @param block_size IO size in bytes
  /// @param alignment  Offsets are aligned
  /// @param seed       PRNG seed.
  explicit OffsetGenerator(AccessPattern pattern, size_t file_size, size_t block_size,
                           size_t alignment, std::uint64_t seed = std::random_device{}());

  /// @brief Returns the next aligned offset to issue IO
  /// @return Byte offset in [0, file_size - block_size], aligned to alignment.
  [[nodiscard]] off_t Next() noexcept;

 private:
  AccessPattern pattern_;
  size_t file_size_;
  size_t block_size_;
  size_t alignment_;
  size_t sequential_offset_ = 0;
  std::mt19937_64 rng_;
  std::uniform_int_distribution<size_t> dist_;
};

}  // namespace cfio

#endif  // CFIO_TELEMETRY_OFFSET_GENERATOR_H_
