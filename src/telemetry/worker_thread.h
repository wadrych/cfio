#ifndef CFIO_TELEMETRY_WORKER_THREAD_H_
#define CFIO_TELEMETRY_WORKER_THREAD_H_

/// @file worker_thread.h
/// @brief Per single job worker

#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

#include "common/types.h"
#include "config/job_config.h"
#include "engine/i_engine_io.h"
#include "telemetry/aligned_buffer.h"
#include "telemetry/io_direction_decider.h"
#include "telemetry/log2_histogram.h"
#include "telemetry/offset_generator.h"

namespace cfio {

/// @brief Runs a benchmark job on its own thread.
class WorkerThread {
 public:
  /// @brief Build a worker and open its engine
  /// @param config  Job configuration
  /// @param engine  Engine for the job
  /// @throws std::system_error if the engine fails to open the target file.
  WorkerThread(JobConfig config, std::unique_ptr<IEngineIO> engine);

  WorkerThread(const WorkerThread&) = delete;
  WorkerThread& operator=(const WorkerThread&) = delete;
  WorkerThread(WorkerThread&&) = delete;
  WorkerThread& operator=(WorkerThread&&) = delete;

  ~WorkerThread() = default;

  /// @brief Run the worker thread
  ///
  /// @param start_barrier  Shared barrier
  /// @param g_running      Global run flag
  void Start(std::barrier<>& start_barrier, const std::atomic<bool>& g_running);

  /// @brief Wait for the worker thread to finish.
  void Join();

  /// @brief Get the latency histogram
  /// @return Completion latency histogram in nanoseconds
  [[nodiscard]] const Log2Histogram<64, std::uint64_t>& Histogram() const noexcept {
    return histogram_;
  }

  /// @brief Get number of completed IO ops
  /// @return Count of successful and failed completions
  [[nodiscard]] std::uint64_t IopsCount() const noexcept {
    return iops_count_.load(std::memory_order_relaxed);
  }

  /// @brief Get total bytes read and written
  /// @return Total transferred bytes
  [[nodiscard]] std::uint64_t BytesTransferred() const noexcept {
    return bytes_transferred_.load(std::memory_order_relaxed);
  }

  /// @brief Get number of failed read ops
  /// @return Count of read errors
  [[nodiscard]] std::uint64_t ReadErrorCount() const noexcept {
    return read_error_count_.load(std::memory_order_relaxed);
  }

  /// @brief Get number of failed write ops
  /// @return Count of write errors
  [[nodiscard]] std::uint64_t WriteErrorCount() const noexcept {
    return write_error_count_.load(std::memory_order_relaxed);
  }

  /// @brief Get the job configuration of this worker
  /// @return The job configuration
  [[nodiscard]] const JobConfig& Config() const noexcept { return config_; }

  /// @brief Check if O_DIRECT stayed effective after the engine opened the file
  /// @return True if the file was opened with O_DIRECT
  [[nodiscard]] bool DirectEffective() const noexcept { return direct_effective_; }

 private:
  /// @brief Thread entry point. Waits at the barrier, then run the IO loop.
  /// @param start_barrier  Barrier for synchronized start
  /// @param g_running      Global run flag
  void Run(std::barrier<>& start_barrier, const std::atomic<bool>& g_running);

  /// @brief IO loop for iodepth 1 (sync/psync engines).
  ///
  /// @param g_running  Global run flag
  void RunSyncLoop(const std::atomic<bool>& g_running);

  /// @brief IO loop for iodepth greater than 1 (libaio/io_uring engines).
  ///
  /// @param g_running  Global run flag
  void RunAsyncLoop(const std::atomic<bool>& g_running);

  /// @brief Build the next IO request, stamp submit time, and submit it.
  void SubmitOne();

  /// @brief Build the next IO request
  ///
  /// @return A request ready to submit
  [[nodiscard]] IORequest GenerateNextIO() noexcept;

  /// @brief Record one completed IO into the histogram and counters.
  ///
  /// @param completion  The completed IO to record.
  /// @param now         Completion timestamp
  void RecordCompletion(const IOCompletion& completion,
                        std::chrono::steady_clock::time_point now) noexcept;

  JobConfig config_;
  std::unique_ptr<IEngineIO> engine_;
  Log2Histogram<64, std::uint64_t> histogram_;
  std::atomic<std::uint64_t> iops_count_{0};
  std::atomic<std::uint64_t> bytes_transferred_{0};
  std::atomic<std::uint64_t> read_error_count_{0};
  std::atomic<std::uint64_t> write_error_count_{0};
  OffsetGenerator offset_gen_;
  IODirectionDecider direction_decider_;
  AlignedBuffer io_buffer_;
  bool direct_effective_ = false;
  std::uint64_t next_request_id_ = 0;
  std::jthread thread_;
};

}  // namespace cfio

#endif  // CFIO_TELEMETRY_WORKER_THREAD_H_
