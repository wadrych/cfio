/// @file buffer_pool.cpp
/// @brief BufferPool implementation

#include "telemetry/buffer_pool.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

namespace cfio {
namespace {

/// @brief Fills a memory block with random bytes.
/// @param data  Start of the block.
/// @param size  Block size in bytes.
void FillRandom(void* data, size_t size) {
  std::mt19937_64 rng(std::random_device{}());
  auto* bytes = static_cast<std::byte*>(data);

  size_t offset = 0;
  while (offset + sizeof(std::uint64_t) <= size) {
    const std::uint64_t word = rng();
    std::memcpy(bytes + offset, &word, sizeof(word));
    offset += sizeof(word);
  }

  if (offset < size) {
    const std::uint64_t word = rng();
    std::memcpy(bytes + offset, &word, size - offset);
  }
}

/// @brief Computes the pool allocation size with an overflow check.
/// @param block_size  Slot size in bytes.
/// @param capacity    Number of slots.
/// @return Total size in bytes.
/// @throws std::invalid_argument if the product overflows size_t.
size_t TotalSize(size_t block_size, size_t capacity) {
  if (block_size != 0 && capacity > std::numeric_limits<size_t>::max() / block_size) {
    throw std::invalid_argument("buffer pool size overflows: block_size " +
                                std::to_string(block_size) + " times capacity " +
                                std::to_string(capacity));
  }
  return block_size * capacity;
}

/// @brief Validates the slot count.
/// @param capacity  Number of slots.
/// @return The capacity unchanged.
/// @throws std::invalid_argument if the capacity is zero or too large to index.
size_t CheckCapacity(size_t capacity) {
  if (capacity == 0) {
    throw std::invalid_argument("buffer pool capacity must be greater than 0");
  }
  if (capacity > std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument("buffer pool capacity too large: " + std::to_string(capacity));
  }
  return capacity;
}

}  // namespace

BufferPool::BufferPool(size_t alignment, size_t block_size, size_t capacity)
    : storage_(alignment, TotalSize(block_size, CheckCapacity(capacity))),
      block_size_(block_size),
      capacity_(capacity) {
  FillRandom(storage_.Data(), storage_.Size());

  free_slots_.reserve(capacity_);
  for (size_t i = capacity_; i > 0; --i) {
    free_slots_.push_back(static_cast<std::uint32_t>(i - 1));
  }

#ifndef NDEBUG
  in_use_.assign(capacity_, 0);
#endif
}

std::uint32_t BufferPool::Acquire() noexcept {
  assert(!free_slots_.empty() && "buffer pool exhausted, more requests in flight than iodepth");

  const std::uint32_t slot = free_slots_.back();
  free_slots_.pop_back();

#ifndef NDEBUG
  in_use_[slot] = 1;
#endif

  return slot;
}

void BufferPool::Release(std::uint32_t slot) noexcept {
  if (static_cast<size_t>(slot) >= capacity_) {
    assert(false && "buffer pool release of an out of range slot");
    return;
  }

#ifndef NDEBUG
  assert(in_use_[slot] == 1 && "buffer pool double release, engine emitted two completions");
  in_use_[slot] = 0;
#endif

  free_slots_.push_back(slot);
}

void* BufferPool::Data(std::uint32_t slot) noexcept {
  if (static_cast<size_t>(slot) >= capacity_) {
    return nullptr;
  }
  auto* bytes = static_cast<std::byte*>(storage_.Data());
  return bytes + (static_cast<size_t>(slot) * block_size_);
}

}  // namespace cfio
