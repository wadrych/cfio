/// @file size_parser.cpp
/// @brief Implementation of the SizeParser utility.

#include "config/size_parser.h"

#include <cctype>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace cfio {

namespace {

// Constants for size suffixes.
constexpr size_t kKilo = 1024;
constexpr size_t kMega = 1024ULL * 1024;
constexpr size_t kGiga = 1024ULL * 1024 * 1024;
}  // namespace

size_t SizeParser::Parse(const std::string& size_str, AllowedSuffixes allowed) {
  if (size_str.empty()) {
    throw std::invalid_argument("size string is empty");
  }

  if (std::isdigit(static_cast<unsigned char>(size_str[0])) == 0) {
    throw std::invalid_argument("size string must start with a digit: '" + size_str + "'");
  }

  // Find where digits end.
  size_t pos = 0;
  while (pos < size_str.size() && std::isdigit(static_cast<unsigned char>(size_str[pos])) != 0) {
    ++pos;
  }

  // Convert the digit substring to a number.
  uint64_t value = 0;
  try {
    value = std::stoull(size_str.substr(0, pos));
  } catch (const std::out_of_range&) {
    throw std::invalid_argument("numeric value out of range: '" + size_str + "'");
  }

  // No suffix — return raw byte count.
  if (pos == size_str.size()) {
    return static_cast<size_t>(value);
  }

  // Exactly one suffix character allowed after the digits.
  if (pos + 1 != size_str.size()) {
    throw std::invalid_argument("unexpected characters after suffix: '" + size_str + "'");
  }

  // Match the suffix (case-insensitive).
  size_t multiplier = 0;
  const char suffix = size_str[pos];
  switch (suffix) {
    case 'k':
    case 'K':
      multiplier = kKilo;
      break;
    case 'm':
    case 'M':
      multiplier = kMega;
      break;
    case 'g':
    case 'G':
      if (allowed == AllowedSuffixes::kKM) {
        throw std::invalid_argument("suffix 'g' not allowed for this field: '" + size_str + "'");
      }
      multiplier = kGiga;
      break;
    default:
      throw std::invalid_argument("unknown size suffix '" + std::string(1, suffix) + "' in: '" +
                                  size_str + "'");
  }

  // Overflow check before multiply.
  if (value > std::numeric_limits<size_t>::max() / multiplier) {
    throw std::invalid_argument("size value overflows: '" + size_str + "'");
  }

  return static_cast<size_t>(value) * multiplier;
}

std::string SizeParser::Format(size_t bytes) {
  if (bytes == 0) {
    return "0";
  }
  if (bytes % kGiga == 0) {
    return std::to_string(bytes / kGiga) + "G";
  }
  if (bytes % kMega == 0) {
    return std::to_string(bytes / kMega) + "M";
  }
  if (bytes % kKilo == 0) {
    return std::to_string(bytes / kKilo) + "K";
  }
  return std::to_string(bytes);
}

}  // namespace cfio
