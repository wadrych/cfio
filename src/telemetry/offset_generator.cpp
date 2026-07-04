/// @file offset_generator.cpp
/// @brief Implementation of OffsetGenerator.

#include "telemetry/offset_generator.h"

namespace cfio {

OffsetGenerator::OffsetGenerator(AccessPattern pattern, size_t file_size, size_t block_size,
                                 size_t alignment, std::uint64_t seed)
    : pattern_(pattern),
      file_size_(file_size),
      block_size_(block_size),
      alignment_(alignment),
      rng_(seed),
      dist_(0, file_size - block_size) {
}

off_t OffsetGenerator::Next() noexcept {
  if (pattern_ == AccessPattern::kSequential) {
    const auto current = static_cast<off_t>(sequential_offset_);
    sequential_offset_ += block_size_;
    if (sequential_offset_ + block_size_ > file_size_) {
      sequential_offset_ = 0;
    }
    return current;
  }

  const size_t raw = dist_(rng_);
  const size_t aligned = (raw / alignment_) * alignment_;
  return static_cast<off_t>(aligned);
}

}  // namespace cfio
