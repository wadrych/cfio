/// @file test_aligned_buffer.cpp
/// @brief Unit tests for AlignedBuffer RAII wrapper.
///

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>

#include <gtest/gtest.h>

#include "telemetry/aligned_buffer.h"

namespace cfio {
namespace {

TEST(AlignedBufferTest, BasicAllocation) {
  AlignedBuffer buf(4096, 4096);
  EXPECT_NE(buf.data(), nullptr);
  EXPECT_EQ(buf.size(), 4096u);
  EXPECT_EQ(buf.alignment(), 4096u);
  EXPECT_FALSE(buf.empty());
}

TEST(AlignedBufferTest, DataIsAligned) {
  constexpr size_t kAlignment = 4096;
  AlignedBuffer buf(kAlignment, kAlignment);
  auto addr = reinterpret_cast<uintptr_t>(buf.data());
  EXPECT_EQ(addr % kAlignment, 0u);
}

TEST(AlignedBufferTest, ConstDataAccessor) {
  const AlignedBuffer buf(4096, 4096);
  const void* ptr = buf.data();
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(buf.size(), 4096u);
  EXPECT_EQ(buf.alignment(), 4096u);
  EXPECT_FALSE(buf.empty());
}

TEST(AlignedBufferTest, BufferIsWritable) {
  AlignedBuffer buf(4096, 4096);
  std::memset(buf.data(), 0xAB, buf.size());
  EXPECT_EQ(static_cast<uint8_t*>(buf.data())[0], 0xABu);
  EXPECT_EQ(static_cast<uint8_t*>(buf.data())[4095], 0xABu);
}

TEST(AlignedBufferTest, MoveConstructor) {
  AlignedBuffer a(4096, 4096);
  void* original_data = a.data();
  size_t original_size = a.size();

  AlignedBuffer b(std::move(a));

  EXPECT_EQ(b.data(), original_data);
  EXPECT_EQ(b.size(), original_size);
  EXPECT_EQ(a.data(), nullptr);
  EXPECT_EQ(a.size(), 0u);
  EXPECT_TRUE(a.empty());
}

TEST(AlignedBufferTest, MoveAssignment) {
  AlignedBuffer a(4096, 4096);
  AlignedBuffer b(4096, 8192);
  void* b_data = b.data();
  size_t b_size = b.size();

  a = std::move(b);

  EXPECT_EQ(a.data(), b_data);
  EXPECT_EQ(a.size(), b_size);
}

TEST(AlignedBufferTest, SelfMoveAssignment) {
  // Self-move via swap is a no-op. We only verify it doesn't crash
  // and the object remains usable — the standard only guarantees
  // a "valid but unspecified" state for moved-from objects.
  AlignedBuffer a(4096, 4096);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
  a = std::move(a);
#pragma GCC diagnostic pop

  // Swap-based implementation preserves state, but we only assert
  // the object is valid and non-empty, not an implementation detail.
  EXPECT_FALSE(a.empty());
  EXPECT_NE(a.data(), nullptr);
  EXPECT_GT(a.size(), 0u);
}

TEST(AlignedBufferTest, ZeroSizeThrows) {
  EXPECT_THROW(AlignedBuffer(4096, 0), std::invalid_argument);
}

TEST(AlignedBufferTest, ZeroAlignmentThrows) {
  EXPECT_THROW(AlignedBuffer(0, 4096), std::invalid_argument);
}

TEST(AlignedBufferTest, NonPowerOfTwoAlignmentThrows) {
  EXPECT_THROW(AlignedBuffer(3, 3), std::invalid_argument);
  EXPECT_THROW(AlignedBuffer(6, 6), std::invalid_argument);
  EXPECT_THROW(AlignedBuffer(100, 100), std::invalid_argument);
}

TEST(AlignedBufferTest, SizeNotMultipleOfAlignmentThrows) {
  EXPECT_THROW(AlignedBuffer(4096, 5000), std::invalid_argument);
  EXPECT_THROW(AlignedBuffer(4096, 4097), std::invalid_argument);
}

TEST(AlignedBufferTest, LargeBuffer) {
  constexpr size_t kAlignment = 4096;
  constexpr size_t kSize = 1048576;
  AlignedBuffer buf(kAlignment, kSize);
  EXPECT_NE(buf.data(), nullptr);
  EXPECT_EQ(buf.size(), kSize);
  auto addr = reinterpret_cast<uintptr_t>(buf.data());
  EXPECT_EQ(addr % kAlignment, 0u);
}

TEST(AlignedBufferTest, MultipleAlignments) {
  // Each size must be a multiple of its alignment.
  constexpr size_t kAlignments[] = {512, 4096, 65536};
  for (size_t alignment : kAlignments) {
    AlignedBuffer buf(alignment, alignment);
    EXPECT_NE(buf.data(), nullptr) << "alignment: " << alignment;
    EXPECT_EQ(buf.size(), alignment) << "alignment: " << alignment;
    EXPECT_EQ(buf.alignment(), alignment) << "alignment: " << alignment;
    auto addr = reinterpret_cast<uintptr_t>(buf.data());
    EXPECT_EQ(addr % alignment, 0u) << "alignment: " << alignment;
  }
}

TEST(AlignedBufferTest, MovedFromObjectDestructs) {
  // After move, the source holds nullptr. Its destructor must not crash.
  // Under ASan, a double-free would be caught here.
  AlignedBuffer a(4096, 4096);
  AlignedBuffer b(std::move(a));
  EXPECT_TRUE(a.empty());
  // a goes out of scope here — calling free on nullptr must be a no-op.
}

TEST(AlignedBufferTest, EmptyAfterMoveConstruct) {
  AlignedBuffer a(4096, 4096);
  AlignedBuffer b(std::move(a));
  EXPECT_TRUE(a.empty());
  EXPECT_EQ(a.data(), nullptr);
  EXPECT_EQ(a.size(), 0u);
  EXPECT_EQ(a.alignment(), 0u);
}

}  // namespace
}  // namespace cfio
