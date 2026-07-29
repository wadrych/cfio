#ifndef CFIO_TELEMETRY_LOG2_HISTOGRAM_H_
#define CFIO_TELEMETRY_LOG2_HISTOGRAM_H_

/// @file log2_histogram.h
/// @brief Log-scale histogram for latency percentile tracking.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace cfio {

/// @brief Fixed-size histogram that buckets values by their power-of-two
///        magnitude.
///
/// @tparam BucketCount Number of buckets. Must be in [1, 64]
/// @tparam CounterType Unsigned integer type used for the per-bucket counters.
template<std::size_t BucketCount = 64, typename CounterType = std::uint64_t>
class Log2Histogram {
 public:
  Log2Histogram() = default;
  ~Log2Histogram() = default;

  Log2Histogram(const Log2Histogram&) = delete;
  Log2Histogram& operator=(const Log2Histogram&) = delete;
  Log2Histogram(Log2Histogram&&) = delete;
  Log2Histogram& operator=(Log2Histogram&&) = delete;

  /// @brief Records one value into bucket
  /// @param value_ns Latency to record, in nanoseconds
  void Record(std::uint64_t value_ns) noexcept {
    const auto raw_index = static_cast<std::size_t>(63 - __builtin_clzll(value_ns | 1ULL));
    const std::size_t index = std::min(raw_index, kMaxBucket);
    buckets_[index].fetch_add(1, std::memory_order_relaxed);
  }

  /// @brief Returns the upper bound of the bucket holding the requested quantile.
  /// @param p Quantile as a fraction in [0.0, 1.0]
  /// @return Upper bound of the target bucket
  [[nodiscard]] std::uint64_t Percentile(double p) const noexcept {
    const CounterType total = TotalCount();
    if (total == 0) {
      return 0;
    }
    const double clamped_p = std::clamp(p, 0.0, 1.0);

    auto target = static_cast<CounterType>(std::ceil(clamped_p * static_cast<double>(total)));
    if (target < 1) {
      target = 1;
    }
    if (target > total) {
      target = total;
    }
    CounterType cumulative = 0;
    for (std::size_t index = 0; index < BucketCount; ++index) {
      cumulative += buckets_[index].load(std::memory_order_relaxed);
      if (cumulative >= target) {
        return BucketUpperBound(index);
      }
    }
    return BucketUpperBound(BucketCount - 1);
  }

  /// @brief Returns the total number of recorded values.
  /// @return Sum of all bucket counters.
  [[nodiscard]] CounterType TotalCount() const noexcept {
    CounterType total = 0;
    for (const auto& bucket : buckets_) {
      total += bucket.load(std::memory_order_relaxed);
    }
    return total;
  }

  /// @brief Returns the lower bound of the first non empty bucket.
  /// @return The smallest observable value, or zero when the histogram is empty.
  [[nodiscard]] std::uint64_t MinValue() const noexcept {
    for (std::size_t index = 0; index < BucketCount; ++index) {
      if (buckets_[index].load(std::memory_order_relaxed) != 0) {
        return BucketLowerBound(index);
      }
    }
    return 0;
  }

  /// @brief Returns the upper bound of the last non empty bucket.
  /// @return The largest observable value, or zero when the histogram is empty.
  [[nodiscard]] std::uint64_t MaxValue() const noexcept {
    for (std::size_t index = BucketCount; index > 0; --index) {
      if (buckets_[index - 1].load(std::memory_order_relaxed) != 0) {
        return BucketUpperBound(index - 1);
      }
    }
    return 0;
  }

  /// @brief Adds another histogram's counts into this one, bucket by bucket.
  /// @param other Histogram to merge from.
  void Merge(const Log2Histogram& other) noexcept {
    for (std::size_t index = 0; index < BucketCount; ++index) {
      const CounterType other_count = other.buckets_[index].load(std::memory_order_relaxed);
      buckets_[index].fetch_add(other_count, std::memory_order_relaxed);
    }
  }

  /// @brief Sets all bucket counters back to zero.
  void Reset() noexcept {
    for (auto& bucket : buckets_) {
      bucket.store(0, std::memory_order_relaxed);
    }
  }

 private:
  static_assert(BucketCount >= 1 && BucketCount <= 64, "BucketCount must be in the range [1, 64]");
  static_assert(std::is_integral_v<CounterType> && std::is_unsigned_v<CounterType>,
                "CounterType must be an uint type");

  /// Index of the last bucket
  static constexpr std::size_t kMaxBucket = BucketCount - 1;

  /// @brief Returns the smallest value that falls into the given bucket
  static std::uint64_t BucketLowerBound(std::size_t index) noexcept {
    return index == 0 ? 0ULL : (1ULL << index);
  }

  /// @brief Returns the largest value that falls into the given bucket
  static std::uint64_t BucketUpperBound(std::size_t index) noexcept {
    if (index >= 63) {
      return UINT64_MAX;
    }
    return (1ULL << (index + 1)) - 1;
  }

  std::array<std::atomic<CounterType>, BucketCount> buckets_{};
};

}  // namespace cfio

#endif  // CFIO_TELEMETRY_LOG2_HISTOGRAM_H_
