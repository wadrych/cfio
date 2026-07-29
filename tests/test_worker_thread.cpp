/// @file test_worker_thread.cpp
/// @brief Unit tests for WorkerThread

#include <atomic>
#include <barrier>
#include <cerrno>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "common/types.h"
#include "config/job_config.h"
#include "engine/i_engine_io.h"
#include "telemetry/worker_thread.h"

namespace cfio {
namespace {

constexpr size_t kBlockSize = 4096;
constexpr size_t kFileSize = 65536;

// Build JobConfig for a worker
JobConfig MakeConfig(const std::string& engine, RWMode rw_mode, int iodepth) {
  JobConfig cfg;
  cfg.name = "worker_test";
  cfg.engine = engine;
  cfg.rw_mode = rw_mode;
  cfg.access_pattern = JobConfig::DeriveAccessPattern(rw_mode);
  cfg.block_size = kBlockSize;
  cfg.file_size = kFileSize;
  cfg.iodepth = iodepth;
  cfg.direct = false;
  cfg.rwmixread = 50;
  cfg.alignment = kBlockSize;
  return cfg;
}

class FakeEngine : public IEngineIO {
 public:
  int completions_per_poll = 1;          // batch size returned per poll
  std::uint64_t stop_after = 0;          // flip running off after N completions
  bool fail_all = false;                 // inject IO errors on every completion
  bool direct_enabled = false;           // reported by IsDirectEnabled
  std::atomic<bool>* running = nullptr;  // shared run flag

  int in_flight = 0;
  int max_in_flight = 0;
  int steady_min_in_flight = std::numeric_limits<int>::max();
  std::uint64_t total_submitted = 0;
  std::uint64_t total_completed = 0;
  std::uint64_t submits_after_stop = 0;

  void Open(const JobConfig& /*config*/) override {}

  void SubmitIO(const IORequest& request) override {
    if (running != nullptr && !running->load(std::memory_order_relaxed)) {
      ++submits_after_stop;
    }
    pending_.push_back(Pending{request.submit_time, request.direction, request.length});
    ++in_flight;
    ++total_submitted;
    max_in_flight = std::max(max_in_flight, in_flight);
  }

  void PollCompletions(int min_events, int max_events, std::vector<IOCompletion>& out) override {
    if (pending_.empty()) {
      return;
    }

    if (running != nullptr && running->load(std::memory_order_relaxed)) {
      steady_min_in_flight = std::min(steady_min_in_flight, in_flight);
    }

    const int available = static_cast<int>(pending_.size());
    int count = std::min({completions_per_poll, max_events, available});
    if (count < min_events) {
      count = std::min(min_events, available);
    }

    for (int i = 0; i < count; ++i) {
      const Pending info = pending_.front();
      pending_.pop_front();
      --in_flight;
      ++total_completed;

      IOCompletion completion{};
      completion.direction = info.direction;
      completion.submit_time = info.submit_time;
      if (fail_all) {
        completion.bytes_transferred = 0;
        completion.success = false;
        completion.error_code = EIO;
      } else {
        completion.bytes_transferred = static_cast<ssize_t>(info.length);
        completion.success = true;
        completion.error_code = 0;
      }
      out.push_back(completion);

      if (stop_after != 0 && total_completed >= stop_after && running != nullptr &&
          running->load(std::memory_order_relaxed)) {
        running->store(false, std::memory_order_relaxed);
      }
    }
  }

  void Close() override {}

  [[nodiscard]] bool IsDirectEnabled() const noexcept override { return direct_enabled; }

 private:
  struct Pending {
    std::chrono::steady_clock::time_point submit_time;
    IODirection direction;
    size_t length;
  };

  std::deque<Pending> pending_;
};

TEST(WorkerThreadTest, SyncLoopCountersMatchCompletions) {
  constexpr std::uint64_t kTarget = 50;
  auto config = MakeConfig("psync", RWMode::kRead, 1);

  auto fake = std::make_unique<FakeEngine>();
  fake->stop_after = kTarget;
  FakeEngine* raw = fake.get();

  std::atomic<bool> g_running{true};
  fake->running = &g_running;
  WorkerThread worker(config, std::move(fake));

  std::barrier<> start_barrier{1};
  worker.Start(start_barrier, g_running);
  worker.Join();

  EXPECT_EQ(worker.IopsCount(), kTarget);
  EXPECT_EQ(worker.BytesTransferred(), kTarget * kBlockSize);
  EXPECT_EQ(worker.Histogram().TotalCount(), kTarget);
  EXPECT_EQ(raw->total_submitted, kTarget);
  EXPECT_EQ(raw->total_completed, kTarget);
  EXPECT_EQ(raw->in_flight, 0);
  EXPECT_EQ(raw->submits_after_stop, 0U);
  EXPECT_EQ(worker.ReadErrorCount(), 0U);
  EXPECT_EQ(worker.WriteErrorCount(), 0U);
}

TEST(WorkerThreadTest, AsyncLoopKeepsQueueFull) {
  constexpr int kDepth = 8;
  constexpr std::uint64_t kTarget = 100;
  auto config = MakeConfig("libaio", RWMode::kRandRead, kDepth);

  auto fake = std::make_unique<FakeEngine>();
  fake->completions_per_poll = 1;
  fake->stop_after = kTarget;
  FakeEngine* raw = fake.get();

  std::atomic<bool> g_running{true};
  fake->running = &g_running;
  WorkerThread worker(config, std::move(fake));

  std::barrier<> start_barrier{1};
  worker.Start(start_barrier, g_running);
  worker.Join();

  EXPECT_EQ(raw->max_in_flight, kDepth);
  EXPECT_EQ(raw->steady_min_in_flight, kDepth);
  EXPECT_GE(worker.IopsCount(), kTarget);
  EXPECT_EQ(worker.IopsCount(), raw->total_completed);
}

TEST(WorkerThreadTest, AsyncLoopDrainsAfterStop) {
  constexpr int kDepth = 8;
  constexpr std::uint64_t kTarget = 60;
  auto config = MakeConfig("io_uring", RWMode::kRandRead, kDepth);

  auto fake = std::make_unique<FakeEngine>();
  fake->completions_per_poll = kDepth;
  fake->stop_after = kTarget;
  FakeEngine* raw = fake.get();

  std::atomic<bool> g_running{true};
  fake->running = &g_running;
  WorkerThread worker(config, std::move(fake));

  std::barrier<> start_barrier{1};
  worker.Start(start_barrier, g_running);
  worker.Join();

  EXPECT_EQ(raw->in_flight, 0);
  EXPECT_EQ(raw->submits_after_stop, 0U);
  EXPECT_EQ(raw->total_submitted, raw->total_completed);
  EXPECT_EQ(worker.IopsCount(), raw->total_completed);
  EXPECT_GE(worker.IopsCount(), kTarget);
}

TEST(WorkerThreadTest, ReadErrorsCountedSeparately) {
  constexpr std::uint64_t kTarget = 30;
  auto config = MakeConfig("psync", RWMode::kRandRead, 1);

  auto fake = std::make_unique<FakeEngine>();
  fake->fail_all = true;
  fake->stop_after = kTarget;

  std::atomic<bool> g_running{true};
  fake->running = &g_running;
  WorkerThread worker(config, std::move(fake));

  std::barrier<> start_barrier{1};
  worker.Start(start_barrier, g_running);
  worker.Join();

  EXPECT_EQ(worker.IopsCount(), kTarget);
  EXPECT_EQ(worker.ReadErrorCount(), kTarget);
  EXPECT_EQ(worker.WriteErrorCount(), 0U);
  EXPECT_EQ(worker.BytesTransferred(), 0U);
}

TEST(WorkerThreadTest, WriteErrorsCountedSeparately) {
  constexpr std::uint64_t kTarget = 30;
  auto config = MakeConfig("psync", RWMode::kWrite, 1);

  auto fake = std::make_unique<FakeEngine>();
  fake->fail_all = true;
  fake->stop_after = kTarget;

  std::atomic<bool> g_running{true};
  fake->running = &g_running;
  WorkerThread worker(config, std::move(fake));

  std::barrier<> start_barrier{1};
  worker.Start(start_barrier, g_running);
  worker.Join();

  EXPECT_EQ(worker.IopsCount(), kTarget);
  EXPECT_EQ(worker.WriteErrorCount(), kTarget);
  EXPECT_EQ(worker.ReadErrorCount(), 0U);
  EXPECT_EQ(worker.BytesTransferred(), 0U);
}

TEST(WorkerThreadTest, ConfigReturnsConfiguredJob) {
  auto config = MakeConfig("libaio", RWMode::kRandRead, 8);
  const WorkerThread worker(config, std::make_unique<FakeEngine>());

  EXPECT_EQ(worker.Config().name, config.name);
  EXPECT_EQ(worker.Config().engine, "libaio");
  EXPECT_EQ(worker.Config().rw_mode, RWMode::kRandRead);
  EXPECT_EQ(worker.Config().iodepth, 8);
}

TEST(WorkerThreadTest, DirectEffectiveReflectsEngineWhenEnabled) {
  constexpr std::uint64_t kTarget = 5;
  auto config = MakeConfig("psync", RWMode::kRead, 1);

  auto fake = std::make_unique<FakeEngine>();
  fake->direct_enabled = true;
  fake->stop_after = kTarget;

  std::atomic<bool> g_running{true};
  fake->running = &g_running;
  WorkerThread worker(config, std::move(fake));

  std::barrier<> start_barrier{1};
  worker.Start(start_barrier, g_running);
  worker.Join();

  EXPECT_TRUE(worker.DirectEffective());
}

TEST(WorkerThreadTest, DirectEffectiveReflectsEngineWhenDisabled) {
  constexpr std::uint64_t kTarget = 5;
  auto config = MakeConfig("psync", RWMode::kRead, 1);

  auto fake = std::make_unique<FakeEngine>();
  fake->direct_enabled = false;
  fake->stop_after = kTarget;

  std::atomic<bool> g_running{true};
  fake->running = &g_running;
  WorkerThread worker(config, std::move(fake));

  std::barrier<> start_barrier{1};
  worker.Start(start_barrier, g_running);
  worker.Join();

  EXPECT_FALSE(worker.DirectEffective());
}

}  // namespace
}  // namespace cfio
