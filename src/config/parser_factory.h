#ifndef CFIO_CONFIG_PARSER_FACTORY_H_
#define CFIO_CONFIG_PARSER_FACTORY_H_

/// @file parser_factory.h
/// @brief Factory that selects the correct config parser by file extension.

#include <filesystem>
#include <memory>

#include "config/i_config_parser.h"

namespace cfio {

/// @brief Creates the appropriate IConfigParser implementation based on the
/// config file extension, either .json or .csv.
class ParserFactory {
 public:
  ParserFactory() = delete;
  /// @brief Create a parser for the given configuration file.
  /// @param config_path Path to the configuration file.
  /// @return A parser capable of reading the file format.
  /// @throws std::invalid_argument if the file extension is not supported.
  static std::unique_ptr<IConfigParser> Create(const std::filesystem::path& config_path);
};

}  // namespace cfio

#endif  // CFIO_CONFIG_PARSER_FACTORY_H_
