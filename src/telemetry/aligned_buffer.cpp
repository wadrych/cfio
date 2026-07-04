/// @file aligned_buffer.cpp
/// @brief Implementation of AlignedBuffer RAII wrapper.

#include "telemetry/aligned_buffer.h"

#include <cstdlib>
#include <new>
#include <stdexcept>
#include <utility>

namespace cfio {

AlignedBuffer::AlignedBuffer(size_t alignment, size_t size)
    : data_(nullptr), size_(size), alignment_(alignment) {
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    throw std::invalid_argument("alignment must be a positive power of 2, got " +
                                std::to_string(alignment));
  }

  if (size == 0) {
    throw std::invalid_argument("size must be greater than 0");
  }

  if (size % alignment != 0) {
    throw std::invalid_argument("size (" + std::to_string(size) +
                                ") must be a multiple of alignment (" + std::to_string(alignment) +
                                "), required by aligned_alloc");
  }

  data_ = std::aligned_alloc(alignment, size);
  if (data_ == nullptr) {
    throw std::bad_alloc();
  }
}

AlignedBuffer::~AlignedBuffer() {
  std::free(data_);
}

AlignedBuffer::AlignedBuffer(AlignedBuffer&& other) noexcept
    : data_(std::exchange(other.data_, nullptr)),
      size_(std::exchange(other.size_, 0)),
      alignment_(std::exchange(other.alignment_, 0)) {
}

AlignedBuffer& AlignedBuffer::operator=(AlignedBuffer&& other) noexcept {
  std::swap(data_, other.data_);
  std::swap(size_, other.size_);
  std::swap(alignment_, other.alignment_);
  return *this;
}

}  // namespace cfio
