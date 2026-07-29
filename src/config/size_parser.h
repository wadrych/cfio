#ifndef CFIO_CONFIG_SIZE_PARSER_H_
#define CFIO_CONFIG_SIZE_PARSER_H_

/// @file size_parser.h
/// @brief Converts human-readable size strings such as "4k" or "1G" to bytes
///        and back.

#include <cstddef>
#include <string>

namespace cfio {

/// @brief Converts between human-readable size strings and byte counts.
class SizeParser {
 public:
  /// @brief Suffix sets accepted when parsing a size string.
  enum class AllowedSuffixes {
    kKM,  ///< Accept k and m only, used for block sizes
    kKMG  ///< Accept k, m and g, used for file sizes
  };

  /// @brief Parse a size string such as "4k" or "1G" into a byte count.
  /// @param size_str  Size string starting with a digit.
  /// @param allowed   Suffix set accepted for this field.
  /// @return The size in bytes.
  /// @throws std::invalid_argument if the string is empty, malformed, uses a
  ///         suffix outside @p allowed, or overflows size_t.
  static size_t Parse(const std::string& size_str, AllowedSuffixes allowed = AllowedSuffixes::kKMG);

  /// @brief Format a byte count back to a human-readable string.
  /// @param bytes  Byte count to format.
  /// @return The formatted size string.
  static std::string Format(size_t bytes);
};

}  // namespace cfio

#endif  // CFIO_CONFIG_SIZE_PARSER_H_
