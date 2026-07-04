/// @file parser_factory.cpp
/// @brief Implementation of the config parser factory.

#include "config/parser_factory.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>

#include "config/csv_parser.h"
#include "config/json_parser.h"

namespace cfio {

std::unique_ptr<IConfigParser> ParserFactory::Create(const std::filesystem::path& config_path) {
  auto ext = config_path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (ext == ".json") {
    return std::make_unique<JsonParser>();
  }
  if (ext == ".csv") {
    return std::make_unique<CsvParser>();
  }

  throw std::invalid_argument("unsupported config file extension: '" + ext + "'");
}

}  // namespace cfio
