#ifndef CFIO_REPORTING_RESULTS_EXPORTER_H_
#define CFIO_REPORTING_RESULTS_EXPORTER_H_

/// @file results_exporter.h
/// @brief Serializes benchmark results to json or csv

#include <filesystem>
#include <vector>

#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {

/// Writes post run benchmark output files
class ResultsExporter {
 public:
  /// @brief Write summary
  /// @param results Final results
  /// @param output_dir Existing directory
  /// @throws std::runtime_error if the output file cannot be opened.
  static void ExportJson(const BenchmarkResults& results,
                         const std::filesystem::path& output_dir);

  /// @brief Write timeseries
  /// @param time_series Per-sample metrics history.
  /// @param output_dir Existing directory
  static void ExportCsv(const std::vector<MetricsSnapshot>& time_series,
                        const std::filesystem::path& output_dir);
};

}  // namespace cfio

#endif  // CFIO_REPORTING_RESULTS_EXPORTER_H_
