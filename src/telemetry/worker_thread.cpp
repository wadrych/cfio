/// @file worker_thread.cpp
/// @brief WorkerThread implementation

#include "telemetry/worker_thread.h"

#include <chrono>
#include <utility>
#include <vector>

#include "engine/engine_factory.h"
#include "logging/logger.h"

namespace cfio {

WorkerThread::WorkerThread(JobConfig config, std::unique_ptr<IEngineIO> engine)
    : config_(std::move(config)),
      engine_(std::move(engine)),
      offset_gen_(config_.access_pattern, config_.file_size, config_.block_size, config_.alignment),
      direction_decider_(config_.rw_mode, config_.rwmixread),
      io_buffer_(config_.alignment, config_.block_size) {
  if (config_.iodepth > 1 && EngineFactory::IsSynchronousEngine(config_.engine)) {
    Logger::get()->warn("job '{}': engine '{}' is synchronous, iodepth {} is ignored", config_.name,
                        config_.engine, config_.iodepth);
  }

  engine_->Open(config_);
  direct_effective_ = engine_->IsDirectEnabled();
}

void WorkerThread::Start(std::barrier<>& start_barrier, const std::atomic<bool>& g_running) {
  thread_ = std::jthread([this, &start_barrier, &g_running]() { Run(start_barrier, g_running); });
}

void WorkerThread::Join() {
  if (thread_.joinable()) {
    thread_.join();
  }
}

void WorkerThread::Run(std::barrier<>& start_barrier, const std::atomic<bool>& g_running) {
  start_barrier.arrive_and_wait();

  const int effective_iodepth = config_.EffectiveIODepth();
  Logger::get()->debug("worker '{}' started, effective iodepth {}", config_.name,
                       effective_iodepth);

  if (effective_iodepth == 1) {
    RunSyncLoop(g_running);
  } else {
    Logger::get()->warn("worker '{}': async IO loop (iodepth {}) not implemented yet", config_.name,
                        effective_iodepth);
  }
}

void WorkerThread::RunSyncLoop(const std::atomic<bool>& g_running) {
  std::vector<IOCompletion> completions;
  completions.reserve(1);

  while (g_running.load(std::memory_order_relaxed)) {
    IORequest request = GenerateNextIO();
    request.submit_time = std::chrono::steady_clock::now();
    engine_->SubmitIO(request);

    completions.clear();
    engine_->PollCompletions(1, 1, completions);

    const auto now = std::chrono::steady_clock::now();
    for (const IOCompletion& completion : completions) {
      RecordCompletion(completion, now);
    }
  }
}

IORequest WorkerThread::GenerateNextIO() noexcept {
  IORequest request;
  request.offset = offset_gen_.Next();
  request.buffer = io_buffer_.data();
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
