#ifndef CFIO_CONFIG_JSON_PARSER_H_
#define CFIO_CONFIG_JSON_PARSER_H_

/// @file json_parser.h
/// @brief JSON configuration file parser.

#include <filesystem>
#include <vector>

#include "config/i_config_parser.h"
#include "config/job_config.h"

namespace cfio {

/// Parses a JSON configuration file into a list of job configurations.
/// Expects the root object to contain a "jobs" array.
class JsonParser : public IConfigParser {
 public:
  /// Parse a JSON configuration file.
  /// @param path Path to the JSON file.
  /// @return A vector of parsed job configurations.
  /// @throws std::runtime_error on I/O errors or malformed JSON.
  /// @throws std::invalid_argument on invalid field values.
  std::vector<JobConfig> Parse(const std::filesystem::path& path) const override;
};

}  // namespace cfio

#endif  // CFIO_CONFIG_JSON_PARSER_H_
