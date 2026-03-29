/// @file json_parser.cpp
/// @brief Implementation of the JSON configuration parser.

#include "config/json_parser.h"

#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "config/size_parser.h"

namespace cfio {

namespace {

/// Parse a single job JSON object into a JobConfig.
JobConfig ParseJob(const nlohmann::json& job, size_t index) {
  JobConfig config;

  try {
    config.name = job.at("name").get<std::string>();
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error("job[" + std::to_string(index) +
                             "]: missing or invalid 'name': " + e.what());
  }

  const std::string ctx = "job[" + std::to_string(index) + "] '" +
                           config.name + "': ";

  try {
    // Required fields.
    config.engine = job.at("engine").get<std::string>();

    const auto rw_str = job.at("rw").get<std::string>();
    config.rw_mode = JobConfig::ParseRWMode(rw_str);
    config.access_pattern = JobConfig::DeriveAccessPattern(config.rw_mode);

    config.block_size = SizeParser::Parse(
        job.at("bs").get<std::string>(), SizeParser::AllowedSuffixes::kKM);
    config.file_size = SizeParser::Parse(
        job.at("size").get<std::string>(), SizeParser::AllowedSuffixes::kKMG);

    // Optional fields with defaults per the config schema.
    config.iodepth = job.value("iodepth", 1);
    config.direct = job.value("direct", true);
    config.rwmixread = job.value("rwmixread", 50);

    const auto filename_str = job.value("filename", std::string{});
    if (filename_str.empty()) {
      config.filename = "./cfio-" + config.name + ".dat";
    } else {
      config.filename = filename_str;
    }

    const auto align_str = job.value("align", std::string{"4k"});
    config.alignment = SizeParser::Parse(
        align_str, SizeParser::AllowedSuffixes::kKM);

  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error(ctx + e.what());
  } catch (const std::invalid_argument& e) {
    throw std::invalid_argument(ctx + e.what());
  } catch (const std::exception& e) {
    throw std::runtime_error(ctx + e.what());
  }

  return config;
}

}  // namespace

std::vector<JobConfig> JsonParser::Parse(
    const std::filesystem::path& path) const {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("cannot open config file: '" +
                             path.string() + "'");
  }

  nlohmann::json root;
  try {
    root = nlohmann::json::parse(file);
  } catch (const nlohmann::json::exception& e) {
    throw std::runtime_error("JSON parse error in '" + path.string() +
                             "': " + e.what());
  }

  // Root must be a JSON object. Anything else (array, number, null) is
  // malformed config.
  if (!root.is_object()) {
    throw std::runtime_error("config root must be a JSON object in '" +
                             path.string() + "'");
  }

  if (!root.contains("jobs")) {
    throw std::runtime_error("missing 'jobs' key in '" +
                             path.string() + "'");
  }

  const auto& jobs_value = root.at("jobs");
  if (!jobs_value.is_array()) {
    throw std::runtime_error("'jobs' must be an array in '" +
                             path.string() + "'");
  }

  std::vector<JobConfig> configs;
  configs.reserve(jobs_value.size());

  for (size_t i = 0; i < jobs_value.size(); ++i) {
    try {
      configs.push_back(ParseJob(jobs_value[i], i));
    } catch (const std::invalid_argument&) {
      throw;
    } catch (const std::runtime_error&) {
      throw;
    } catch (const std::exception& e) {
      // Catch-all for any unexpected exception type.
      throw std::runtime_error("job[" + std::to_string(i) + "]: " +
                               e.what());
    }
  }

  return configs;
}

}  // namespace cfio
