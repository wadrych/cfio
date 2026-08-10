/// @file metric_format.cpp
/// @brief Implementation of the shared metric formatting helpers

#include "display/metric_format.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include <fmt/format.h>

namespace cfio {
namespace {

constexpr int kSecPerMin = 60;

}  // namespace

std::string FormatCount(std::uint64_t value) {
  const std::string digits = std::to_string(value);
  const std::size_t count = digits.size();
  std::string out;
  out.reserve(count + ((count - 1) / 3));
  for (std::size_t i = 0; i < count; ++i) {
    if (i != 0 && (count - i) % 3 == 0) {
      out.push_back(',');
    }
    out.push_back(digits[i]);
  }
  return out;
}

std::string FormatBytes(std::uint64_t bytes) {
  const auto value = static_cast<double>(bytes);
  if (bytes >= kBytesPerGiB) {
    return fmt::format("{:.1f} GiB", value / static_cast<double>(kBytesPerGiB));
  }
  if (bytes >= kBytesPerMiB) {
    return fmt::format("{:.1f} MiB", value / static_cast<double>(kBytesPerMiB));
  }
  if (bytes >= kBytesPerKiB) {
    return fmt::format("{:.1f} KiB", value / static_cast<double>(kBytesPerKiB));
  }
  return fmt::format("{} B", bytes);
}

std::uint64_t RateMiB(std::uint64_t bytes_per_sec) {
  return bytes_per_sec / kBytesPerMiB;
}

std::uint64_t LatencyUs(std::uint64_t nanoseconds) {
  return nanoseconds / kNsPerUs;
}

std::string FormatRate(std::uint64_t bytes_per_sec) {
  return fmt::format("{} MB/s", RateMiB(bytes_per_sec));
}

std::string FormatLatencyUs(std::uint64_t nanoseconds) {
  return fmt::format("{} {}s", LatencyUs(nanoseconds), kMicroSign);
}

std::string FormatDuration(int seconds) {
  const int clamped = std::max(0, seconds);
  return fmt::format("{:02}:{:02}", clamped / kSecPerMin, clamped % kSecPerMin);
}

}  // namespace cfio
