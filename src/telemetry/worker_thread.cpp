/// @file worker_thread.cpp
/// @brief WorkerThread implementation

#include "telemetry/worker_thread.h"

#include <chrono>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "engine/engine_factory.h"
#include "logging/logger.h"

namespace cfio {

WorkerThread::WorkerThread(JobConfig config, std::unique_ptr<IEngineIO> engine)
    : config_(std::move(config)),
      io_buffer_(config_.alignment, config_.block_size),
      engine_(std::move(engine)),
      offset_gen_(config_.access_pattern, config_.file_size, config_.block_size, config_.alignment),
      direction_decider_(config_.rw_mode, config_.rwmixread) {
  if (config_.iodepth > 1 && EngineFactory::IsSynchronousEngine(config_.engine)) {
    Logger::Get()->warn("job '{}': engine '{}' is synchronous, iodepth {} is ignored", config_.name,
                        config_.engine, config_.iodepth);
  }

  engine_->Open(config_);
  direct_effective_ = engine_->IsDirectEnabled();
}

void WorkerThread::Start(std::barrier<>& start_barrier, std::atomic<bool>& g_running) {
  thread_ = std::jthread([this, &start_barrier, &g_running]() { Run(start_barrier, g_running); });
}

void WorkerThread::Join() {
  if (thread_.joinable()) {
    thread_.join();
  }
}

void WorkerThread::Run(std::barrier<>& start_barrier, std::atomic<bool>& g_running) {
  try {
    start_barrier.arrive_and_wait();

    const int effective_iodepth = config_.EffectiveIODepth();
    Logger::Get()->debug("worker '{}' started, effective iodepth {}", config_.name,
                         effective_iodepth);

    if (effective_iodepth == 1) {
      RunSyncLoop(g_running);
    } else {
      RunAsyncLoop(g_running);
    }
  } catch (const std::exception& e) {
    RecordFailure(e.what(), g_running);
  } catch (...) {
    RecordFailure("unknown exception", g_running);
  }
}

void WorkerThread::RecordFailure(const char* what, std::atomic<bool>& g_running) noexcept {
  try {
    error_message_ = what;
  } catch (...) {
    error_message_.clear();
  }
  failed_.store(true, std::memory_order_release);
  g_running.store(false, std::memory_order_relaxed);

  try {
    Logger::Get()->error("worker '{}' failed: {}", config_.name, what);
    // NOLINTNEXTLINE(bugprone-empty-catch)
  } catch (...) {
  }
}

const std::string& WorkerThread::ErrorMessage() const noexcept {
  static const std::string kEmpty;
  if (!failed_.load(std::memory_order_acquire)) {
    return kEmpty;
  }
  return error_message_;
}

void WorkerThread::RunSyncLoop(const std::atomic<bool>& g_running) {
  std::vector<IOCompletion> completions;
  completions.reserve(1);

  while (g_running.load(std::memory_order_relaxed)) {
    SubmitOne();

    completions.clear();
    engine_->PollCompletions(1, 1, completions);

    const auto now = std::chrono::steady_clock::now();
    for (const IOCompletion& completion : completions) {
      RecordCompletion(completion, now);
    }
  }
}

void WorkerThread::RunAsyncLoop(const std::atomic<bool>& g_running) {
  const int effective_iodepth = config_.EffectiveIODepth();

  std::vector<IOCompletion> completions;
  completions.reserve(static_cast<size_t>(effective_iodepth));

  int in_flight = 0;

  // pre-fill the queue to iodepth
  for (int i = 0; i < effective_iodepth && g_running.load(std::memory_order_relaxed); ++i) {
    SubmitOne();
    ++in_flight;
  }

  // drain completions, refill immediately to keep the queue full
  while (g_running.load(std::memory_order_relaxed) || in_flight > 0) {
    completions.clear();
    engine_->PollCompletions(1, effective_iodepth, completions);

    const auto now = std::chrono::steady_clock::now();
    for (const IOCompletion& completion : completions) {
      RecordCompletion(completion, now);
      --in_flight;

      if (g_running.load(std::memory_order_relaxed)) {
        SubmitOne();
        ++in_flight;
      }
    }
  }
}

void WorkerThread::SubmitOne() {
  IORequest request = GenerateNextIO();
  request.submit_time = std::chrono::steady_clock::now();
  engine_->SubmitIO(request);
}

IORequest WorkerThread::GenerateNextIO() noexcept {
  IORequest request;
  request.offset = offset_gen_.Next();
  request.buffer = io_buffer_.Data();
  request.length = config_.block_size;
  request.direction = direction_decider_.Next();
  request.id = next_request_id_++;
  return request;
}

void WorkerThread::RecordCompletion(const IOCompletion& completion,
                                    std::chrono::steady_clock::time_point now) noexcept {
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now - completion.submit_time).count();
  histogram_.Record(elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0);

  iops_count_.fetch_add(1, std::memory_order_relaxed);
  bytes_transferred_.fetch_add(static_cast<std::uint64_t>(completion.bytes_transferred),
                               std::memory_order_relaxed);

  if (!completion.success) {
    auto& error_counter =
        completion.direction == IODirection::kRead ? read_error_count_ : write_error_count_;
    error_counter.fetch_add(1, std::memory_order_relaxed);
  }
}

}  // namespace cfio
