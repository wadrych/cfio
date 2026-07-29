#ifndef CFIO_TELEMETRY_ALIGNED_BUFFER_H_
#define CFIO_TELEMETRY_ALIGNED_BUFFER_H_

/// @file aligned_buffer.h
/// @brief RAII wrapper for aligned memory allocation, used for O_DIRECT buffers.

#include <cstddef>

namespace cfio {

/// @brief Owns a block of memory aligned for O_DIRECT IO.
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
  /// @param other  Buffer to move from.
  AlignedBuffer(AlignedBuffer&& other) noexcept;

  /// @brief Move assignment. Swaps contents so the old resource is freed
  ///        when the source is destroyed. Swap guarantees self-assignment
  ///        safety without a branch.
  /// @param other  Buffer to move from.
  /// @return Reference to this buffer.
  AlignedBuffer& operator=(AlignedBuffer&& other) noexcept;

  AlignedBuffer(const AlignedBuffer&) = delete;
  AlignedBuffer& operator=(const AlignedBuffer&) = delete;

  /// @brief Returns a mutable pointer to the aligned memory.
  /// @return Pointer to the allocation, or nullptr if the buffer is empty.
  void* Data() noexcept { return data_; }

  /// @brief Returns a const pointer to the aligned memory.
  /// @return Pointer to the allocation, or nullptr if the buffer is empty.
  [[nodiscard]] const void* Data() const noexcept { return data_; }

  /// @brief Returns the buffer size in bytes.
  /// @return Size in bytes, zero if the buffer is empty.
  [[nodiscard]] size_t Size() const noexcept { return size_; }

  /// @brief Returns the alignment in bytes.
  /// @return Alignment in bytes, zero if the buffer is empty.
  [[nodiscard]] size_t Alignment() const noexcept { return alignment_; }

  /// @brief Returns true if the buffer holds no allocation, e.g. moved from.
  /// @return True if there is no allocation.
  [[nodiscard]] bool Empty() const noexcept { return data_ == nullptr; }

 private:
  void* data_ = nullptr;
  size_t size_ = 0;
  size_t alignment_ = 0;
};

}  // namespace cfio

#endif  // CFIO_TELEMETRY_ALIGNED_BUFFER_H_
