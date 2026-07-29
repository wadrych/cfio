/// @file test_aligned_buffer.cpp
/// @brief Unit tests for AlignedBuffer RAII wrapper.
///

#include <array>
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
  EXPECT_NE(buf.Data(), nullptr);
  EXPECT_EQ(buf.Size(), 4096U);
  EXPECT_EQ(buf.Alignment(), 4096U);
  EXPECT_FALSE(buf.Empty());
}

TEST(AlignedBufferTest, DataIsAligned) {
  constexpr size_t kAlignment = 4096;
  AlignedBuffer buf(kAlignment, kAlignment);
  auto addr = reinterpret_cast<uintptr_t>(buf.Data());
  EXPECT_EQ(addr % kAlignment, 0U);
}

TEST(AlignedBufferTest, ConstDataAccessor) {
  const AlignedBuffer buf(4096, 4096);
  const void* ptr = buf.Data();
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(buf.Size(), 4096U);
  EXPECT_EQ(buf.Alignment(), 4096U);
  EXPECT_FALSE(buf.Empty());
}

TEST(AlignedBufferTest, BufferIsWritable) {
  AlignedBuffer buf(4096, 4096);
  std::memset(buf.Data(), 0xAB, buf.Size());
  EXPECT_EQ(static_cast<uint8_t*>(buf.Data())[0], 0xABU);
  EXPECT_EQ(static_cast<uint8_t*>(buf.Data())[4095], 0xABU);
}

TEST(AlignedBufferTest, MoveConstructor) {
  AlignedBuffer a(4096, 4096);
  void* original_data = a.Data();
  size_t const original_size = a.Size();

  AlignedBuffer b(std::move(a));

  EXPECT_EQ(b.Data(), original_data);
  EXPECT_EQ(b.Size(), original_size);
  // NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
  EXPECT_EQ(a.Data(), nullptr);
  EXPECT_EQ(a.Size(), 0U);
  EXPECT_TRUE(a.Empty());
  // NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
}

TEST(AlignedBufferTest, MoveAssignment) {
  AlignedBuffer a(4096, 4096);
  AlignedBuffer b(4096, 8192);
  void* b_data = b.Data();
  size_t const b_size = b.Size();

  a = std::move(b);

  EXPECT_EQ(a.Data(), b_data);
  EXPECT_EQ(a.Size(), b_size);
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
  EXPECT_FALSE(a.Empty());
  EXPECT_NE(a.Data(), nullptr);
  EXPECT_GT(a.Size(), 0U);
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
  EXPECT_NE(buf.Data(), nullptr);
  EXPECT_EQ(buf.Size(), kSize);
  auto addr = reinterpret_cast<uintptr_t>(buf.Data());
  EXPECT_EQ(addr % kAlignment, 0U);
}

TEST(AlignedBufferTest, MultipleAlignments) {
  // Each size must be a multiple of its alignment.
  constexpr std::array<size_t, 3> kAlignments = {512, 4096, 65536};
  for (size_t const alignment : kAlignments) {
    AlignedBuffer buf(alignment, alignment);
    EXPECT_NE(buf.Data(), nullptr) << "alignment: " << alignment;
    EXPECT_EQ(buf.Size(), alignment) << "alignment: " << alignment;
    EXPECT_EQ(buf.Alignment(), alignment) << "alignment: " << alignment;
    auto addr = reinterpret_cast<uintptr_t>(buf.Data());
    EXPECT_EQ(addr % alignment, 0U) << "alignment: " << alignment;
  }
}

TEST(AlignedBufferTest, MovedFromObjectDestructs) {
  // After move, the source holds nullptr. Its destructor must not crash.
  // Under ASan, a double-free would be caught here.
  AlignedBuffer a(4096, 4096);
  AlignedBuffer const b(std::move(a));
  // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
  EXPECT_TRUE(a.Empty());
  // a goes out of scope here — calling free on nullptr must be a no-op.
}

TEST(AlignedBufferTest, EmptyAfterMoveConstruct) {
  AlignedBuffer a(4096, 4096);
  AlignedBuffer const b(std::move(a));
  // NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
  EXPECT_TRUE(a.Empty());
  EXPECT_EQ(a.Data(), nullptr);
  EXPECT_EQ(a.Size(), 0U);
  EXPECT_EQ(a.Alignment(), 0U);
  // NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
}

}  // namespace
}  // namespace cfio
