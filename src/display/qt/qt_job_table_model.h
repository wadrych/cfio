#ifndef CFIO_DISPLAY_QT_QT_JOB_TABLE_MODEL_H_
#define CFIO_DISPLAY_QT_QT_JOB_TABLE_MODEL_H_

/// @file qt_job_table_model.h
/// @brief Qt free row building for the live metrics table

#include <cstdint>
#include <string>
#include <vector>

#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {

/// @brief Column order of the live metrics table
enum class JobTableColumn : std::uint8_t {
  kJob,          ///< Job name
  kIopsInstant,  ///< IOPS over the last second
  kIopsAverage,  ///< IOPS since the run started
  kBwInstant,    ///< Bandwidth over the last second
  kBwAverage,    ///< Bandwidth since the run started
  kLatP50,       ///< Median latency
  kLatP95,       ///< 95th percentile latency
  kLatP99,       ///< 99th percentile latency
  kErrors        ///< Read plus write errors
};

constexpr int kJobTableColumnCount = 9;

constexpr const char* kJobTableNoValue = "-";

/// @brief One rendered table row
struct JobTableRow {
  std::vector<std::string> cells;  ///< One formatted entry per column
  std::string error_detail;        ///< Read and write error split, for a tooltip
  bool is_total{};                 ///< True for the aggregate row
};

/// @brief Header text of every column
///
/// @return Labels in column order, kJobTableColumnCount entries.
[[nodiscard]] std::vector<std::string> JobTableHeaders();

/// @brief Build one row per job plus the aggregate row
/// @param snapshot  Sample to render.
/// @return Job rows in snapshot order followed by the total row.
[[nodiscard]] std::vector<JobTableRow> BuildJobTableRows(const MetricsSnapshot& snapshot);

/// @brief Build the totals line shown once a run finished
/// @param results  Final result set.
/// @return Summed IOPS, bandwidth, transferred bytes and errors, followed by a stop label.
[[nodiscard]] std::string BuildRunSummary(const BenchmarkResults& results);

}  // namespace cfio

#endif  // CFIO_DISPLAY_QT_QT_JOB_TABLE_MODEL_H_
