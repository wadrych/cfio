#ifndef CFIO_CONFIG_I_CONFIG_PARSER_H_
#define CFIO_CONFIG_I_CONFIG_PARSER_H_

/// @file i_config_parser.h
/// @brief Abstract interface for parsing configuration files into jobs.

#include <filesystem>
#include <vector>

#include "config/job_config.h"

namespace cfio {

/// @brief Abstract interface for parsing a config file into a list of job
/// configurations.
class IConfigParser {
 public:
  virtual ~IConfigParser() = default;

  IConfigParser(const IConfigParser&) = delete;
  IConfigParser& operator=(const IConfigParser&) = delete;
  IConfigParser(IConfigParser&&) = delete;
  IConfigParser& operator=(IConfigParser&&) = delete;

  /// @brief Parse a configuration file and return one JobConfig per job entry.
  /// @param path Path to the configuration file.
  /// @return A vector of parsed job configurations.
  /// @throws std::runtime_error on I/O errors or malformed content.
  /// @throws std::invalid_argument on invalid field values.
  [[nodiscard]] virtual std::vector<JobConfig> Parse(const std::filesystem::path& path) const = 0;

 protected:
  IConfigParser() = default;
};

}  // namespace cfio

#endif  // CFIO_CONFIG_I_CONFIG_PARSER_H_
