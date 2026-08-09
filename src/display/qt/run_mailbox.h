#ifndef CFIO_DISPLAY_QT_RUN_MAILBOX_H_
#define CFIO_DISPLAY_QT_RUN_MAILBOX_H_

/// @file run_mailbox.h
/// @brief Snapshot handoff between the benchmark thread and the GUI thread

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>

#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {

/// @brief Stage of the run as seen by the GUI
enum class Phase : std::uint8_t {
  kIdle,     ///< No run started yet
  kRunning,  ///< Run in progress
  kFinished  ///< Run over, results may be available
};

/// @brief Mutex protected cell holding the newest metrics for the GUI.
class RunMailbox {
 public:
  RunMailbox() = default;
  ~RunMailbox() = default;

  RunMailbox(const RunMailbox&) = delete;
  RunMailbox& operator=(const RunMailbox&) = delete;
  RunMailbox(RunMailbox&&) = delete;
  RunMailbox& operator=(RunMailbox&&) = delete;

  /// @brief Record the run duration and enter the running phase
  /// @param runtime_seconds  Configured run duration
  void SetRuntime(int runtime_seconds);

  /// @brief Store the newest sample and advance the sequence
  /// @param snapshot  Latest metrics
  void PublishSnapshot(const MetricsSnapshot& snapshot);

  /// @brief Store the final results, enter the finished phase and advance the sequence
  /// @param results  Full result set
  void PublishResults(const BenchmarkResults& results);

  /// @brief Enter the finished phase without results
  void MarkShutdown();

  /// @brief Check whether the GUI asked for an early stop
  /// @return true once RequestStop has been called
  [[nodiscard]] bool StopRequested() const;

  /// @brief Read the change counter
  /// @return Number of data publications so far
  [[nodiscard]] std::uint64_t Sequence() const;

  /// @brief Copy the newest sample
  /// @return The last published snapshot, empty if none was published
  [[nodiscard]] MetricsSnapshot LatestSnapshot() const;

  /// @brief Move the results out of the mailbox
  /// @return The results on the first call after PublishResults, nullopt otherwise
  [[nodiscard]] std::optional<BenchmarkResults> TakeResults();

  /// @brief Read the current phase
  /// @return The stage the run is in
  [[nodiscard]] Phase CurrentPhase() const;

  /// @brief Read the configured run duration
  /// @return Run duration in seconds, 0 before SetRuntime
  [[nodiscard]] int RuntimeSeconds() const;

  /// @brief Ask the benchmark thread to stop early
  void RequestStop();

 private:
  mutable std::mutex mutex_;  ///< Guards every non atomic member

  MetricsSnapshot latest_;                   ///< Newest published sample
  std::optional<BenchmarkResults> results_;  ///< Final results until taken
  Phase phase_{Phase::kIdle};                ///< Current run stage
  int runtime_seconds_{0};                   ///< Configured run duration

  std::atomic<std::uint64_t> sequence_{0};   ///< Bumped on every data publish
  std::atomic<bool> stop_requested_{false};  ///< Set by the GUI, read by the worker
};

}  // namespace cfio

#endif  // CFIO_DISPLAY_QT_RUN_MAILBOX_H_
