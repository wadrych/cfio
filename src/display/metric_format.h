#ifndef CFIO_DISPLAY_METRIC_FORMAT_H_
#define CFIO_DISPLAY_METRIC_FORMAT_H_

/// @file metric_format.h
/// @brief Shared metric formatting helpers used by every display backend

#include <cstdint>
#include <string>

namespace cfio {

/// Bytes in one kibibyte.
constexpr std::uint64_t kBytesPerKiB = 1024ULL;
/// Bytes in one mebibyte.
constexpr std::uint64_t kBytesPerMiB = 1024ULL * 1024ULL;
/// Bytes in one gibibyte.
constexpr std::uint64_t kBytesPerGiB = 1024ULL * 1024ULL * 1024ULL;
/// Nanoseconds in one microsecond.
constexpr std::uint64_t kNsPerUs = 1000ULL;

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

/// @brief Format a duration as minutes and seconds
///
/// @param seconds  Duration in seconds, negative values clamp to zero.
/// @return The duration as mm:ss, for example "01:15".
std::string FormatDuration(int seconds);

}  // namespace cfio

#endif  // CFIO_DISPLAY_METRIC_FORMAT_H_
