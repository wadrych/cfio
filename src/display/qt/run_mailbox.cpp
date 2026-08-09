/// @file run_mailbox.cpp
/// @brief Implementation of the GUI metrics handoff

#include "display/qt/run_mailbox.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {

void RunMailbox::SetRuntime(int runtime_seconds) {
  const std::lock_guard<std::mutex> lock(mutex_);
  runtime_seconds_ = runtime_seconds;
  phase_ = Phase::kRunning;
}

void RunMailbox::PublishSnapshot(const MetricsSnapshot& snapshot) {
  const std::lock_guard<std::mutex> lock(mutex_);
  latest_ = snapshot;
  sequence_.fetch_add(1, std::memory_order_release);
}

void RunMailbox::PublishResults(const BenchmarkResults& results) {
  const std::lock_guard<std::mutex> lock(mutex_);
  results_ = results;
  phase_ = Phase::kFinished;
  sequence_.fetch_add(1, std::memory_order_release);
}

void RunMailbox::MarkShutdown() {
  const std::lock_guard<std::mutex> lock(mutex_);
  phase_ = Phase::kFinished;
}

bool RunMailbox::StopRequested() const {
  return stop_requested_.load(std::memory_order_acquire);
}

std::uint64_t RunMailbox::Sequence() const {
  return sequence_.load(std::memory_order_acquire);
}

MetricsSnapshot RunMailbox::LatestSnapshot() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return latest_;
}

std::optional<BenchmarkResults> RunMailbox::TakeResults() {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (!results_.has_value()) {
    return std::nullopt;
  }
  std::optional<BenchmarkResults> taken = std::move(results_);
  results_.reset();
  return taken;
}

Phase RunMailbox::CurrentPhase() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return phase_;
}

int RunMailbox::RuntimeSeconds() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return runtime_seconds_;
}

void RunMailbox::RequestStop() {
  stop_requested_.store(true, std::memory_order_release);
}

}  // namespace cfio
