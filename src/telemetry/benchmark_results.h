#ifndef CFIO_TELEMETRY_BENCHMARK_RESULTS_H_
#define CFIO_TELEMETRY_BENCHMARK_RESULTS_H_

/// @file benchmark_results.h
/// @brief Final results after a benchmark completes

#include <cstdint>
#include <string>
#include <vector>

#include "common/cli_options.h"
#include "config/job_config.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {

/// @brief Cumulative results for a single job over the whole run
struct JobResults {
  std::string name;           ///< Job name from the configuration
  JobConfig config;           ///< Configuration this job ran with
  uint64_t iops_avg{};        ///< Average IOPS over the run
  uint64_t bw_avg_bytes{};    ///< Average bandwidth
  uint64_t lat_min_ns{};      ///< Lowest observed completion latency
  uint64_t lat_max_ns{};      ///< Highest observed completion latency
  uint64_t lat_p50_ns{};      ///< Median completion latency
  uint64_t lat_p95_ns{};      ///< 95th percentile completion latency
  uint64_t lat_p99_ns{};      ///< 99th percentile completion latency
  uint64_t total_ios{};       ///< Total completed IO operations
  uint64_t total_bytes{};     ///< Total bytes transferred
  uint64_t read_errors{};     ///< Error read operations
  uint64_t write_errors{};    ///< Error write operations
  bool direct_effective{};    ///< True if O_DIRECT on
  bool failed{};              ///< True if the worker stopped on an engine error
  std::string error_message;  ///< Failure description, empty when failed is false
};

/// @brief Full result set for a benchmark
struct BenchmarkResults {
  std::string cfio_version;                  ///< Tool version string
  std::string timestamp;                     ///< Run start time
  int runtime_seconds{};                     ///< Configured run duration
  double elapsed_seconds{};                  ///< Measured run duration
  bool interrupted{};                        ///< True when the run stopped before the deadline
  CliOptions global_config;                  ///< Global runtime options
  std::vector<JobResults> jobs;              ///< Final per-job results
  std::vector<MetricsSnapshot> time_series;  ///< Per-sample history
};

}  // namespace cfio

#endif  // CFIO_TELEMETRY_BENCHMARK_RESULTS_H_
