#ifndef CFIO_TELEMETRY_BUFFER_POOL_H_
#define CFIO_TELEMETRY_BUFFER_POOL_H_

/// @file buffer_pool.h
/// @brief Pool of aligned IO buffers, one slot per in-flight request.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "telemetry/aligned_buffer.h"

namespace cfio {

/// @brief Hands out one distinct aligned buffer per in-flight IO request.
class BufferPool {
 public:
  /// @brief Allocates the backing memory and fills it with random bytes.
  /// @param alignment   Alignment in bytes. Must be a positive power of 2.
  /// @param block_size  Slot size in bytes. Must be a positive multiple of
  ///                    alignment, which keeps every slot aligned.
  /// @param capacity    Number of slots. Must be > 0.
  /// @throws std::invalid_argument if a precondition is violated or if
  ///         capacity * block_size overflows size_t.
  /// @throws std::bad_alloc if allocation fails.
  BufferPool(size_t alignment, size_t block_size, size_t capacity);

  ~BufferPool() = default;

  /// @brief Move constructor. Transfers ownership, leaves source empty.
  /// @param other  Pool to move from.
  BufferPool(BufferPool&& other) noexcept = default;

  /// @brief Move assignment. Transfers ownership.
  /// @param other  Pool to move from.
  /// @return Reference to this pool.
  BufferPool& operator=(BufferPool&& other) noexcept = default;

  BufferPool(const BufferPool&) = delete;
  BufferPool& operator=(const BufferPool&) = delete;

  /// @brief Takes a free slot out of the pool.
  /// @return Index of the acquired slot.
  [[nodiscard]] std::uint32_t Acquire() noexcept;

  /// @brief Returns a slot to the pool.
  /// @param slot  Index previously returned by Acquire.
  void Release(std::uint32_t slot) noexcept;

  /// @brief Gets the memory of one slot.
  /// @param slot  Index previously returned by Acquire.
  /// @return Pointer to the slot, or nullptr if the index is out of range.
  [[nodiscard]] void* Data(std::uint32_t slot) noexcept;

  /// @brief Gets the number of slots in the pool.
  /// @return Slot count.
  [[nodiscard]] size_t Capacity() const noexcept { return capacity_; }

  /// @brief Gets the slot size.
  /// @return Slot size in bytes.
  [[nodiscard]] size_t BlockSize() const noexcept { return block_size_; }

  /// @brief Gets the number of slots currently free.
  /// @return Count of free slots.
  [[nodiscard]] size_t Available() const noexcept { return free_slots_.size(); }

  /// @brief Gets the total memory held by the pool.
  /// @return Allocation size in bytes.
  [[nodiscard]] size_t TotalBytes() const noexcept { return storage_.Size(); }

 private:
  AlignedBuffer storage_;
  size_t block_size_ = 0;
  size_t capacity_ = 0;
  std::vector<std::uint32_t> free_slots_;
#ifndef NDEBUG
  std::vector<std::uint8_t> in_use_;
#endif
};

}  // namespace cfio

#endif  // CFIO_TELEMETRY_BUFFER_POOL_H_
