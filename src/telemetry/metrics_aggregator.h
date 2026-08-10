#ifndef CFIO_TELEMETRY_METRICS_AGGREGATOR_H_
#define CFIO_TELEMETRY_METRICS_AGGREGATOR_H_

/// @file metrics_aggregator.h
/// @brief Periodic collector of metrics

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "common/cli_options.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"
#include "telemetry/worker_thread.h"

namespace cfio {

/// @brief Get samples from worker and builds the time series.
class MetricsAggregator {
 public:
  /// @brief Build an aggregator over the given workers.
  /// @param workers          Tracked workers
  /// @param runtime_seconds  Run duration
  MetricsAggregator(std::vector<WorkerThread*> workers, int runtime_seconds);

  MetricsAggregator(const MetricsAggregator&) = delete;
  MetricsAggregator& operator=(const MetricsAggregator&) = delete;
  MetricsAggregator(MetricsAggregator&&) = delete;
  MetricsAggregator& operator=(MetricsAggregator&&) = delete;

  ~MetricsAggregator() = default;

  /// @brief Launch the sampling thread
  /// @param g_running  Global run flag
  void Start(const std::atomic<bool>& g_running);

  /// @brief Stop and join the sampling thread
  void Stop();

  /// @brief Take one last snapshot for the display, recording it in the time
  ///        series when it covers at least half a record interval
  void TakeFinalSnapshot();

  /// @brief Get the most recent snapshot
  /// @return The latest sample, empty if none has been taken yet
  [[nodiscard]] MetricsSnapshot LatestSnapshot() const;

  /// @brief Get the time series in chronological order
  /// @return All samples collected so far, oldest first
  [[nodiscard]] std::vector<MetricsSnapshot> TimeSeries() const;

  /// @brief Build final results from worker totals and the time series
  /// @param opts  Global options
  /// @return Populated results
  [[nodiscard]] BenchmarkResults BuildResults(const CliOptions& opts) const;

 private:
  friend class MetricsAggregatorTestPeer;  // test access

  /// @brief Sampling loop. Wakes every 500ms until stop or run flag clears
  /// @param stop       Stop token from the jthread
  /// @param g_running  Global run flag
  void Run(const std::stop_token& stop, const std::atomic<bool>& g_running);

  /// @brief Read every worker and build one snapshot. Not thread safe, run it
  ///        on the sampling thread or on any thread after Stop
  /// @param record_ts  When true, advance the diff basis to this sample
  /// @return The computed snapshot
  [[nodiscard]] MetricsSnapshot TakeSnapshot(bool record_ts);

  /// @brief Append a snapshot to the circular time series buffer.
  void RecordTimeSeries(const MetricsSnapshot& snapshot);

  std::vector<WorkerThread*> workers_;

  std::size_t ts_capacity_;                   ///< Circular buffer size
  std::vector<MetricsSnapshot> time_series_;  ///< Time series ring
  std::size_t ts_write_index_{0};             ///< Next write slot
  std::size_t ts_count_{0};                   ///< Total rows recorded

  std::vector<std::uint64_t> prev_iops_;   ///< Per worker iops at last record
  std::vector<std::uint64_t> prev_bytes_;  ///< Per worker bytes at last record

  std::chrono::steady_clock::time_point start_time_;        ///< Run start
  std::chrono::steady_clock::time_point prev_sample_time_;  ///< Last record time
  std::chrono::steady_clock::time_point end_time_;          ///< Set by Stop
  std::chrono::system_clock::time_point wall_start_time_;   ///< Start time

  int runtime_seconds_;  ///< Set run duration

  MetricsSnapshot latest_snapshot_;        ///< Newest snapshot for display
  mutable std::mutex snapshot_mutex_;      ///< Guards latest_snapshot_ only
  std::condition_variable_any sample_cv_;  ///< Interruptible tick wait
  std::mutex sample_mutex_;

  std::jthread thread_;  ///< Sampling thread
  bool stopped_{false};  ///< True once Stop has joined the sampling thread
};

}  // namespace cfio

#endif  // CFIO_TELEMETRY_METRICS_AGGREGATOR_H_
