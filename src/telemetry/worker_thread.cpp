/// @file worker_thread.cpp
/// @brief WorkerThread implementation

#include "telemetry/worker_thread.h"

#include <utility>

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
    Logger::get()->warn(
        "job '{}': engine '{}' is synchronous, iodepth {} is ignored",
        config_.name, config_.engine, config_.iodepth);
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

void WorkerThread::Run(std::barrier<>& start_barrier,
                       [[maybe_unused]] const std::atomic<bool>& g_running) {
  start_barrier.arrive_and_wait();

  Logger::get()->debug("worker '{}' started, effective iodepth {}", config_.name,
                       config_.EffectiveIODepth());
}

}  // namespace cfio
