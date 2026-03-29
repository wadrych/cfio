#ifndef CFIO_CONFIG_SIZE_PARSER_H_
#define CFIO_CONFIG_SIZE_PARSER_H_

/// @file size_parser.h
/// @brief Converts human-readable size strings (e.g. "4k", "1G") to bytes
///        and back.

#include <cstddef>
#include <string>

namespace cfio {

/// Converts between human-readable size strings and byte counts.
class SizeParser {
 public:
  /// Allowed suffixes for parsing size strings.
  enum class AllowedSuffixes { kKM, kKMG };

  /// Parse a size string like "4k" or "1G" into a byte count.
  /// Supported suffixes: k (1024), m (1024^2), g (1024^3).
  static size_t Parse(const std::string& size_str, AllowedSuffixes allowed = AllowedSuffixes::kKMG);

  /// Format a byte count back to a human-readable string.
  /// Uses the largest clean unit (M > K > raw bytes).
  static std::string Format(size_t bytes);
};

}  // namespace cfio

#endif  // CFIO_CONFIG_SIZE_PARSER_H_
