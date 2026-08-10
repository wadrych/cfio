#ifndef CFIO_DISPLAY_METRIC_FORMAT_H_
#define CFIO_DISPLAY_METRIC_FORMAT_H_

/// @file metric_format.h
/// @brief Shared metric formatting helpers used by every display backend

#include <cstdint>
#include <string>
#include <string_view>

namespace cfio {

/// Bytes in one kibibyte.
constexpr std::uint64_t kBytesPerKiB = 1024ULL;
/// Bytes in one mebibyte.
constexpr std::uint64_t kBytesPerMiB = 1024ULL * 1024ULL;
/// Bytes in one gibibyte.
constexpr std::uint64_t kBytesPerGiB = 1024ULL * 1024ULL * 1024ULL;
/// Nanoseconds in one microsecond.
constexpr std::uint64_t kNsPerUs = 1000ULL;
/// Micro sign used by every latency label.
constexpr std::string_view kMicroSign = "μ";

/// @brief Format an integer with thousands separators
///
/// @param value  Value to format.
/// @return The value with commas every three digits, for example "125,432".
std::string FormatCount(std::uint64_t value);

/// @brief Format a byte count using binary units
///
/// @param bytes  Byte count to format.
/// @return The largest fitting unit with one decimal, for example "27.0 GiB".
std::string FormatBytes(std::uint64_t bytes);

/// @brief Convert a byte rate to whole mebibytes per second
///
/// @param bytes_per_sec  Rate in bytes per second.
/// @return The rate truncated to whole MiB/s.
[[nodiscard]] std::uint64_t RateMiB(std::uint64_t bytes_per_sec);

/// @brief Convert a nanosecond latency to whole microseconds
///
/// @param nanoseconds  Latency in nanoseconds.
/// @return The latency truncated to whole microseconds.
[[nodiscard]] std::uint64_t LatencyUs(std::uint64_t nanoseconds);

/// @brief Format a byte rate with its unit
///
/// @param bytes_per_sec  Rate in bytes per second.
/// @return The rate in whole mebibytes per second, for example "512 MB/s".
std::string FormatRate(std::uint64_t bytes_per_sec);

/// @brief Format a latency with its unit
///
/// @param nanoseconds  Latency in nanoseconds.
/// @return The latency in whole microseconds, for example "45 μs".
std::string FormatLatencyUs(std::uint64_t nanoseconds);

/// @brief Format a duration as minutes and seconds
///
/// @param seconds  Duration in seconds, negative values clamp to zero.
/// @return The duration as mm:ss, for example "01:15".
std::string FormatDuration(int seconds);

}  // namespace cfio

#endif  // CFIO_DISPLAY_METRIC_FORMAT_H_
