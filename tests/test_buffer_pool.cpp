/// @file test_buffer_pool.cpp
/// @brief Unit tests for BufferPool

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "telemetry/buffer_pool.h"

namespace cfio {
namespace {

constexpr size_t kAlignment = 4096;
constexpr size_t kBlockSize = 4096;
constexpr size_t kCapacity = 32;

TEST(BufferPoolTest, BasicProperties) {
  const BufferPool pool(kAlignment, kBlockSize, kCapacity);
  EXPECT_EQ(pool.Capacity(), kCapacity);
  EXPECT_EQ(pool.BlockSize(), kBlockSize);
  EXPECT_EQ(pool.Available(), kCapacity);
  EXPECT_EQ(pool.TotalBytes(), kBlockSize * kCapacity);
}

TEST(BufferPoolTest, AcquireReturnsDistinctSlots) {
  BufferPool pool(kAlignment, kBlockSize, kCapacity);
  std::set<std::uint32_t> slots;

  for (size_t i = 0; i < kCapacity; ++i) {
    slots.insert(pool.Acquire());
  }

  EXPECT_EQ(slots.size(), kCapacity);
  EXPECT_EQ(pool.Available(), 0U);
}

TEST(BufferPoolTest, AcquireReturnsDistinctPointers) {
  BufferPool pool(kAlignment, kBlockSize, kCapacity);
  std::set<const void*> pointers;

  for (size_t i = 0; i < kCapacity; ++i) {
    const std::uint32_t slot = pool.Acquire();
    void* data = pool.Data(slot);
    ASSERT_NE(data, nullptr);
    pointers.insert(data);
  }

  EXPECT_EQ(pointers.size(), kCapacity);
}

TEST(BufferPoolTest, SlotsAreAligned) {
  BufferPool pool(kAlignment, kBlockSize, kCapacity);

  const auto* base = static_cast<const std::byte*>(pool.Data(0));
  ASSERT_NE(base, nullptr);

  for (std::uint32_t slot = 0; slot < kCapacity; ++slot) {
    const auto* data = static_cast<const std::byte*>(pool.Data(slot));
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(data) % kAlignment, 0U);
    EXPECT_EQ(data - base, static_cast<ptrdiff_t>(slot * kBlockSize));
  }
}

TEST(BufferPoolTest, SlotsDoNotOverlap) {
  BufferPool pool(kAlignment, kBlockSize, 4);

  for (std::uint32_t slot = 0; slot < 4; ++slot) {
    std::memset(pool.Data(slot), static_cast<int>(slot) + 1, kBlockSize);
  }

  for (std::uint32_t slot = 0; slot < 4; ++slot) {
    const auto* data = static_cast<const std::uint8_t*>(pool.Data(slot));
    EXPECT_EQ(data[0], slot + 1);
    EXPECT_EQ(data[kBlockSize - 1], slot + 1);
  }
}

TEST(BufferPoolTest, OutOfRangeSlotHasNoData) {
  BufferPool pool(kAlignment, kBlockSize, 2);
  EXPECT_EQ(pool.Data(2), nullptr);
  EXPECT_EQ(pool.Data(99), nullptr);
}

TEST(BufferPoolTest, ReleaseMakesSlotReusable) {
  BufferPool pool(kAlignment, kBlockSize, 2);

  const std::uint32_t first = pool.Acquire();
  const std::uint32_t second = pool.Acquire();
  EXPECT_EQ(pool.Available(), 0U);

  pool.Release(first);
  EXPECT_EQ(pool.Available(), 1U);

  const std::uint32_t reused = pool.Acquire();
  EXPECT_EQ(reused, first);
  EXPECT_NE(reused, second);
}

TEST(BufferPoolTest, ReleaseIsLifo) {
  BufferPool pool(kAlignment, kBlockSize, 4);

  const std::uint32_t a = pool.Acquire();
  const std::uint32_t b = pool.Acquire();
  const std::uint32_t c = pool.Acquire();

  pool.Release(a);
  pool.Release(c);

  EXPECT_EQ(pool.Acquire(), c);
  EXPECT_EQ(pool.Acquire(), a);
  EXPECT_NE(b, a);
}

TEST(BufferPoolTest, AvailableTracksAcquireRelease) {
  BufferPool pool(kAlignment, kBlockSize, kCapacity);
  std::vector<std::uint32_t> held;

  for (size_t i = 0; i < 10; ++i) {
    held.push_back(pool.Acquire());
    EXPECT_EQ(pool.Available(), kCapacity - held.size());
  }

  while (!held.empty()) {
    pool.Release(held.back());
    held.pop_back();
    EXPECT_EQ(pool.Available(), kCapacity - held.size());
  }

  EXPECT_EQ(pool.Available(), kCapacity);
}

TEST(BufferPoolTest, CapacityOneBehavesLikeSingleBuffer) {
  BufferPool pool(kAlignment, kBlockSize, 1);

  for (int i = 0; i < 5; ++i) {
    const std::uint32_t slot = pool.Acquire();
    EXPECT_EQ(slot, 0U);
    EXPECT_NE(pool.Data(slot), nullptr);
    EXPECT_EQ(pool.Available(), 0U);
    pool.Release(slot);
  }

  EXPECT_EQ(pool.Available(), 1U);
}

TEST(BufferPoolTest, ZeroCapacityThrows) {
  EXPECT_THROW(BufferPool(kAlignment, kBlockSize, 0), std::invalid_argument);
}

TEST(BufferPoolTest, OverflowingSizeThrows) {
  constexpr size_t kHuge = std::numeric_limits<size_t>::max() / 2;
  EXPECT_THROW(BufferPool(kAlignment, kHuge, 4), std::invalid_argument);
}

TEST(BufferPoolTest, BadAlignmentThrows) {
  EXPECT_THROW(BufferPool(0, kBlockSize, 4), std::invalid_argument);
  EXPECT_THROW(BufferPool(3000, kBlockSize, 4), std::invalid_argument);
}

TEST(BufferPoolTest, BlockSizeNotMultipleOfAlignmentThrows) {
  EXPECT_THROW(BufferPool(kAlignment, 5000, 4), std::invalid_argument);
}

TEST(BufferPoolTest, BuffersAreNotAllZero) {
  BufferPool pool(kAlignment, kBlockSize, 4);

  for (std::uint32_t slot = 0; slot < 4; ++slot) {
    const auto* data = static_cast<const std::uint8_t*>(pool.Data(slot));
    ASSERT_NE(data, nullptr);

    bool all_zero = true;
    for (size_t i = 0; i < kBlockSize; ++i) {
      if (data[i] != 0) {
        all_zero = false;
        break;
      }
    }
    EXPECT_FALSE(all_zero);
  }
}

TEST(BufferPoolTest, BuffersDifferBetweenSlots) {
  BufferPool pool(kAlignment, kBlockSize, 2);
  EXPECT_NE(std::memcmp(pool.Data(0), pool.Data(1), kBlockSize), 0);
}

TEST(BufferPoolTest, MoveTransfersOwnership) {
  BufferPool source(kAlignment, kBlockSize, 4);
  const void* base = source.Data(0);
  const std::uint32_t slot = source.Acquire();

  BufferPool moved(std::move(source));
  EXPECT_EQ(moved.Capacity(), 4U);
  EXPECT_EQ(moved.Available(), 3U);
  EXPECT_EQ(moved.Data(0), base);

  moved.Release(slot);
  EXPECT_EQ(moved.Available(), 4U);
}

}  // namespace
}  // namespace cfio
