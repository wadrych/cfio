#ifndef CFIO_ORCHESTRATOR_BENCHMARK_ORCHESTRATOR_H_
#define CFIO_ORCHESTRATOR_BENCHMARK_ORCHESTRATOR_H_

/// @file benchmark_orchestrator.h
/// @brief Top level coordinator that runs the full benchmark lifecycle

#include <atomic>
#include <filesystem>
#include <memory>
#include <vector>

#include "common/cli_options.h"
#include "config/job_config.h"
#include "display/display_context.h"
#include "display/i_display.h"
#include "orchestrator/file_preparator.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_aggregator.h"
#include "telemetry/worker_thread.h"

namespace cfio {

/// @brief Orchestrates workloads execution.
///
class BenchmarkOrchestrator {
 public:
  /// @brief Build an orchestrator.
  /// @param options  Global runtime options
  /// @param jobs     Parsed and validated job configs
  BenchmarkOrchestrator(CliOptions options, std::vector<JobConfig> jobs);

  BenchmarkOrchestrator(const BenchmarkOrchestrator&) = delete;
  BenchmarkOrchestrator& operator=(const BenchmarkOrchestrator&) = delete;
  BenchmarkOrchestrator(BenchmarkOrchestrator&&) = delete;
  BenchmarkOrchestrator& operator=(BenchmarkOrchestrator&&) = delete;

  ~BenchmarkOrchestrator() = default;

  /// @brief Run the whole benchmark.
  /// @return EXIT_SUCCESS on success, EXIT_FAILURE on setup failure
  int Run();

 private:
  /// @brief Resolve and create the output directory.
  void SetupOutputDirectory();

  /// @brief Move logging into the output directory.
  void InitLogging();

  /// @brief Precreate and fill every job file.
  void PrecreateFiles();

  /// @brief Build one engine and worker per job.
  void CreateWorkers();

  /// @brief Start aggregator, launch workers, run the timer, collect results.
  /// @param results  Filled with the final result set
  void RunBenchmark(BenchmarkResults& results);

  /// @brief Write summary and time series files.
  /// @param results  Final results
  void ExportResults(const BenchmarkResults& results);

  /// @brief Delete test files unless keep_files.
  void CleanupFiles();

  /// @brief Build display header metadata from the config.
  [[nodiscard]] DisplayContext BuildDisplayContext() const;

  CliOptions options_;
  std::vector<JobConfig> jobs_;
  std::filesystem::path output_dir_;
  FilePreparator file_prep_;
  std::vector<std::unique_ptr<WorkerThread>> workers_;
  std::unique_ptr<MetricsAggregator> aggregator_;
  std::unique_ptr<IDisplay> display_;
  std::atomic<bool> g_running_{true};
  bool logging_ready_ = false;
};

}  // namespace cfio

#endif  // CFIO_ORCHESTRATOR_BENCHMARK_ORCHESTRATOR_H_
