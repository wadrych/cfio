/// @file csv_parser.cpp
/// @brief Implementation of the CSV configuration parser.

#include "config/csv_parser.h"

#include <csv.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "config/size_parser.h"

namespace cfio {

namespace {

constexpr const char* kRequiredColumns[] = {"name", "engine", "rw", "bs", "size"};

/// Parse an integer from a CSV string.
int ParseInt(const std::string& str, const std::string& field_name) {
  size_t pos = 0;
  int value = 0;
  try {
    value = std::stoi(str, &pos);
  } catch (const std::out_of_range&) {
    throw std::invalid_argument("'" + field_name + "' value out of range: '" + str + "'");
  } catch (const std::invalid_argument&) {
    throw std::invalid_argument("'" + field_name + "' is not a valid integer: '" + str + "'");
  }
  if (pos != str.size()) {
    throw std::invalid_argument("'" + field_name + "' has trailing characters: '" + str + "'");
  }
  return value;
}

/// Parse a boolean from a CSV string (case-insensitive).
bool ParseBool(const std::string& str, const std::string& field_name) {
  std::string lower = str;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (lower == "true" || lower == "1") {
    return true;
  }
  if (lower == "false" || lower == "0") {
    return false;
  }
  throw std::invalid_argument("'" + field_name + "' must be true/false/1/0, got: '" + str + "'");
}

/// Parse a single CSV row into a JobConfig.
JobConfig ParseJob(const csv::CSVRow& row, size_t index,
                   const std::unordered_set<std::string>& columns) {
  JobConfig config;

  // Get optional field value.
  auto get_opt = [&](const std::string& field) -> std::string {
    if (columns.count(field) == 0) {
      return "";
    }
    return row[field].get<std::string>();
  };

  // Parse name first for error context.
  try {
    config.name = row["name"].get<std::string>();
  } catch (const std::exception& e) {
    throw std::runtime_error("row[" + std::to_string(index) +
                             "]: error reading 'name': " + e.what());
  }
  if (config.name.empty()) {
    throw std::runtime_error("row[" + std::to_string(index) + "]: 'name' is empty");
  }

  const std::string ctx = "row[" + std::to_string(index) + "] '" + config.name + "': ";

  try {
    // Required string fields.
    config.engine = row["engine"].get<std::string>();
    if (config.engine.empty()) {
      throw std::runtime_error("'engine' is empty");
    }

    auto rw_str = row["rw"].get<std::string>();
    if (rw_str.empty()) {
      throw std::runtime_error("'rw' is empty");
    }
    config.rw_mode = JobConfig::ParseRWMode(rw_str);
    config.access_pattern = JobConfig::DeriveAccessPattern(config.rw_mode);

    auto bs_str = row["bs"].get<std::string>();
    if (bs_str.empty()) {
      throw std::runtime_error("'bs' is empty");
    }
    config.block_size = SizeParser::Parse(bs_str, SizeParser::AllowedSuffixes::kKM);

    auto size_str = row["size"].get<std::string>();
    if (size_str.empty()) {
      throw std::runtime_error("'size' is empty");
    }
    config.file_size = SizeParser::Parse(size_str, SizeParser::AllowedSuffixes::kKMG);

    // Optional fields with defaults.
    auto iodepth_str = get_opt("iodepth");
    config.iodepth = iodepth_str.empty() ? 1 : ParseInt(iodepth_str, "iodepth");

    auto direct_str = get_opt("direct");
    config.direct = direct_str.empty() ? true : ParseBool(direct_str, "direct");

    auto rwmix_str = get_opt("rwmixread");
    config.rwmixread = rwmix_str.empty() ? 50 : ParseInt(rwmix_str, "rwmixread");

    auto filename_str = get_opt("filename");
    if (filename_str.empty()) {
      config.filename = "./cfio-" + config.name + ".dat";
    } else {
      config.filename = filename_str;
    }

    auto align_str = get_opt("align");
    if (align_str.empty()) {
      config.alignment = SizeParser::Parse("4k", SizeParser::AllowedSuffixes::kKM);
    } else {
      config.alignment = SizeParser::Parse(align_str, SizeParser::AllowedSuffixes::kKM);
    }

  } catch (const std::invalid_argument& e) {
    throw std::invalid_argument(ctx + e.what());
  } catch (const std::exception& e) {
    throw std::runtime_error(ctx + e.what());
  }

  return config;
}

}  // namespace

std::vector<JobConfig> CsvParser::Parse(const std::filesystem::path& path) const {
  csv::CSVReader reader = [&]() {
    try {
      csv::CSVFormat format;
      format.variable_columns(csv::VariableColumnPolicy::KEEP);
      return csv::CSVReader(path.string(), format);
    } catch (const std::exception& e) {
      throw std::runtime_error("cannot open config file: '" + path.string() + "': " + e.what());
    }
  }();

  // Build column set for optional field existence checks.
  const auto col_names = reader.get_col_names();
  const std::unordered_set<std::string> columns(col_names.begin(), col_names.end());

  // Verify all required columns exist in header.
  for (const auto* required : kRequiredColumns) {
    if (columns.count(required) == 0) {
      throw std::runtime_error("missing required column '" + std::string(required) + "' in '" +
                               path.string() + "'");
    }
  }

  std::vector<JobConfig> configs;
  size_t row_index = 0;

  for (auto& row : reader) {
    try {
      configs.push_back(ParseJob(row, row_index, columns));
    } catch (const std::invalid_argument&) {
      throw;
    } catch (const std::runtime_error&) {
      throw;
    } catch (const std::exception& e) {
      throw std::runtime_error("row[" + std::to_string(row_index) + "]: " + e.what());
    }
    ++row_index;
  }

  return configs;
}

}  // namespace cfio
