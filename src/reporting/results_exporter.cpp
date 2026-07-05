/// @file results_exporter.cpp
/// @brief Implementation of the benchmark results exporter

#include "reporting/results_exporter.h"

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "common/cli_options.h"
#include "config/job_config.h"
#include "logging/logger.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {

namespace {

nlohmann::ordered_json JobConfigToJson(const JobConfig& config) {
  nlohmann::ordered_json out;
  out["engine"] = config.engine;
  out["rw"] = JobConfig::ToString(config.rw_mode);
  out["bs"] = config.block_size;
  out["size"] = config.file_size;
  out["iodepth"] = config.iodepth;
  return out;
}

nlohmann::ordered_json JobResultsToJson(const JobResults& results) {
  nlohmann::ordered_json errors;
  errors["read"] = results.read_errors;
  errors["write"] = results.write_errors;

  nlohmann::ordered_json out;
  out["iops_avg"] = results.iops_avg;
  out["bw_avg_bytes"] = results.bw_avg_bytes;
  out["lat_min_ns"] = results.lat_min_ns;
  out["lat_max_ns"] = results.lat_max_ns;
  out["lat_p50_ns"] = results.lat_p50_ns;
  out["lat_p95_ns"] = results.lat_p95_ns;
  out["lat_p99_ns"] = results.lat_p99_ns;
  out["total_ios"] = results.total_ios;
  out["total_bytes"] = results.total_bytes;
  out["errors"] = std::move(errors);
  return out;
}

nlohmann::ordered_json GlobalConfigToJson(const CliOptions& opts) {
  nlohmann::ordered_json out;
  if (opts.engine_override.has_value()) {
    out["engine_override"] = *opts.engine_override;
  } else {
    out["engine_override"] = nullptr;
  }
  if (opts.direct_override.has_value()) {
    out["direct"] = *opts.direct_override;
  } else {
    out["direct"] = nullptr;
  }
  return out;
}

}  // namespace

void ResultsExporter::ExportJson(const BenchmarkResults& results,
                                 const std::filesystem::path& output_dir) {
  nlohmann::ordered_json root;
  root["cfio_version"] = results.cfio_version;
  root["timestamp"] = results.timestamp;
  root["runtime_seconds"] = results.runtime_seconds;
  root["global_config"] = GlobalConfigToJson(results.global_config);

  nlohmann::ordered_json jobs = nlohmann::ordered_json::array();
  for (const JobResults& job : results.jobs) {
    nlohmann::ordered_json job_json;
    job_json["name"] = job.name;
    job_json["config"] = JobConfigToJson(job.config);
    job_json["results"] = JobResultsToJson(job);
    jobs.push_back(std::move(job_json));
  }
  root["jobs"] = std::move(jobs);

  const std::filesystem::path out_path = output_dir / "summary.json";
  std::ofstream out(out_path);
  if (!out.is_open()) {
    throw std::runtime_error("cannot open output file: '" + out_path.string() + "'");
  }

  out << root.dump(2) << '\n';
  out.flush();
  if (!out.good()) {
    throw std::runtime_error("failed writing output file: '" + out_path.string() + "'");
  }

  Logger::get()->info("wrote summary to '{}'", out_path.string());
}

void ResultsExporter::ExportCsv(const std::vector<MetricsSnapshot>& time_series,
                                const std::filesystem::path& output_dir) {

  (void)time_series;
  (void)output_dir;
  throw std::logic_error("ResultsExporter::ExportCsv is not implemented yet");
}

}  // namespace cfio
