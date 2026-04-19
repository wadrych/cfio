/// @file aligned_buffer.h
/// @brief RAII wrapper for aligned memory allocation, used for O_DIRECT buffers.

#ifndef CFIO_TELEMETRY_ALIGNED_BUFFER_H_
#define CFIO_TELEMETRY_ALIGNED_BUFFER_H_

#include <cstddef>

namespace cfio {

/// @brief Owns a block of memory aligned for O_DIRECT IO.
///
class AlignedBuffer {
 public:
  /// @brief Allocates an aligned memory block.
  /// @param alignment Alignment in bytes. Must be a positive power of 2.
  /// @param size      Buffer size in bytes. Must be > 0 and a multiple of
  ///                  alignment.
  /// @note  Argument order matches the aligned_alloc C API.
  /// @throws std::invalid_argument if preconditions are violated.
  /// @throws std::bad_alloc if allocation fails.
  explicit AlignedBuffer(size_t alignment, size_t size);

  /// @brief Frees the aligned memory block.
  ~AlignedBuffer();

  /// @brief Move constructor. Transfers ownership, leaves source empty.
  AlignedBuffer(AlignedBuffer&& other) noexcept;

  /// @brief Move assignment. Swaps contents so the old resource is freed
  ///        when the source is destroyed. Swap guarantees self-assignment
  ///        safety without a branch.
  AlignedBuffer& operator=(AlignedBuffer&& other) noexcept;

  AlignedBuffer(const AlignedBuffer&) = delete;
  AlignedBuffer& operator=(const AlignedBuffer&) = delete;

  /// @brief Returns a mutable pointer to the aligned memory.
  void* data() noexcept { return data_; }

  /// @brief Returns a const pointer to the aligned memory.
  const void* data() const noexcept { return data_; }

  /// @brief Returns the buffer size in bytes.
  [[nodiscard]] size_t size() const noexcept { return size_; }

  /// @brief Returns the alignment in bytes.
  [[nodiscard]] size_t alignment() const noexcept { return alignment_; }

  /// @brief Returns true if the buffer holds no allocation, e.g. moved-from.
  [[nodiscard]] bool empty() const noexcept { return data_ == nullptr; }

 private:
  void* data_ = nullptr;
  size_t size_ = 0;
  size_t alignment_ = 0;
};

}  // namespace cfio

#endif  // CFIO_TELEMETRY_ALIGNED_BUFFER_H_
