#ifndef CFIO_CONFIG_CSV_PARSER_H_
#define CFIO_CONFIG_CSV_PARSER_H_

/// @file csv_parser.h
/// @brief CSV configuration file parser.

#include <filesystem>
#include <vector>

#include "config/i_config_parser.h"
#include "config/job_config.h"

namespace cfio {

/// Parses a CSV configuration file into a list of job configurations.
/// Expects a header row with field names matching the JSON schema, followed
/// by one data row per job. Empty fields use defaults.
class CsvParser : public IConfigParser {
 public:
  /// Parse a CSV configuration file.
  /// @param path Path to the CSV file.
  /// @return A vector of parsed job configurations.
  /// @throws std::runtime_error on I/O errors or malformed CSV.
  /// @throws std::invalid_argument on invalid field values.
  std::vector<JobConfig> Parse(const std::filesystem::path& path) const override;
};

}  // namespace cfio

#endif  // CFIO_CONFIG_CSV_PARSER_H_
