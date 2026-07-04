/// @file metrics_snapshot.h
/// @brief Timestamp performance metrics for live report

#ifndef CFIO_TELEMETRY_METRICS_SNAPSHOT_H_
#define CFIO_TELEMETRY_METRICS_SNAPSHOT_H_

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace cfio {

/// Live performance metrics
struct PerJobMetrics {
  std::string job_name;
  uint64_t iops_instant{};     ///< IOPS over the last one-second window
  uint64_t iops_cumulative{};  ///< Average IOPS since the run started
  uint64_t bw_instant{};       ///< Bandwidth over the last second
  uint64_t bw_cumulative{};    ///< Average bandwidth since start
  uint64_t lat_p50_ns{};       ///< Median completion latency
  uint64_t lat_p95_ns{};       ///< 95th percentile completion latency
  uint64_t lat_p99_ns{};       ///< 99th percentile completion latency
  uint64_t read_errors{};      ///< Error read operations
  uint64_t write_errors{};     ///< Error write operations
};

struct MetricsSnapshot {
  std::chrono::steady_clock::time_point timestamp;  ///< Sample time
  std::vector<PerJobMetrics> jobs;                  ///< Per job metrics
  PerJobMetrics aggregate;                          ///< Sum of all jobs
};

}  // namespace cfio

#endif  // CFIO_TELEMETRY_METRICS_SNAPSHOT_H_
