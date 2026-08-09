/// @file test_worker_thread.cpp
/// @brief Unit tests for WorkerThread

#include <atomic>
#include <barrier>
#include <cerrno>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <system_error>
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

  std::uint64_t throw_on_submit_at = 0;  // throw from the Nth SubmitIO, 0 disables
  std::uint64_t throw_on_poll_at = 0;    // throw from the Nth PollCompletions, 0 disables
  bool throw_non_std = false;            // throw a non std exception instead

  int in_flight = 0;
  int max_in_flight = 0;
  int steady_min_in_flight = std::numeric_limits<int>::max();
  std::uint64_t total_submitted = 0;
  std::uint64_t total_completed = 0;
  std::uint64_t submits_after_stop = 0;
  std::uint64_t submit_calls = 0;
  std::uint64_t poll_calls = 0;
  bool duplicate_in_flight_buffer = false;  // two in-flight requests shared a buffer
  std::set<const void*> distinct_buffers;   // every buffer pointer ever submitted

  void Open(const JobConfig& /*config*/) override {}

  void SubmitIO(const IORequest& request) override {
    ++submit_calls;
    if (throw_on_submit_at != 0 && submit_calls == throw_on_submit_at) {
      ThrowInjected();
    }
    if (running != nullptr && !running->load(std::memory_order_relaxed)) {
      ++submits_after_stop;
    }
    for (const Pending& outstanding : pending_) {
      if (outstanding.buffer == request.buffer) {
        duplicate_in_flight_buffer = true;
      }
    }
    distinct_buffers.insert(request.buffer);

    pending_.push_back(Pending{request.id, request.buffer, request.submit_time, request.direction,
                               request.length});
    ++in_flight;
    ++total_submitted;
    max_in_flight = std::max(max_in_flight, in_flight);
  }

  void PollCompletions(int min_events, int max_events, std::vector<IOCompletion>& out) override {
    ++poll_calls;
    if (throw_on_poll_at != 0 && poll_calls == throw_on_poll_at) {
      ThrowInjected();
    }
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
      completion.id = info.id;
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
  void ThrowInjected() const {
    if (throw_non_std) {
      throw 42;
    }
    throw std::system_error(EAGAIN, std::system_category(), "injected");
  }

  struct Pending {
    std::uint64_t id;
    const void* buffer;
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

TEST(WorkerThreadTest, SyncLoopSurvivesSubmitException) {
  auto config = MakeConfig("psync", RWMode::kRead, 1);

  auto fake = std::make_unique<FakeEngine>();
  fake->throw_on_submit_at = 5;

  std::atomic<bool> g_running{true};
  fake->running = &g_running;
  WorkerThread worker(config, std::move(fake));

  std::barrier<> start_barrier{1};
  worker.Start(start_barrier, g_running);
  worker.Join();

  EXPECT_TRUE(worker.Failed());
  EXPECT_NE(worker.ErrorMessage().find("injected"), std::string::npos);
  EXPECT_FALSE(g_running.load(std::memory_order_relaxed));
  EXPECT_EQ(worker.IopsCount(), 4U);
}

TEST(WorkerThreadTest, AsyncLoopSurvivesPollException) {
  constexpr int kDepth = 8;
  auto config = MakeConfig("libaio", RWMode::kRandRead, kDepth);

  auto fake = std::make_unique<FakeEngine>();
  fake->completions_per_poll = 1;
  fake->throw_on_poll_at = 3;

  std::atomic<bool> g_running{true};
  fake->running = &g_running;
  WorkerThread worker(config, std::move(fake));

  std::barrier<> start_barrier{1};
  worker.Start(start_barrier, g_running);
  worker.Join();

  EXPECT_TRUE(worker.Failed());
  EXPECT_NE(worker.ErrorMessage().find("injected"), std::string::npos);
  EXPECT_FALSE(g_running.load(std::memory_order_relaxed));
  EXPECT_EQ(worker.IopsCount(), 2U);
}

TEST(WorkerThreadTest, NonStdExceptionRecorded) {
  auto config = MakeConfig("psync", RWMode::kRead, 1);

  auto fake = std::make_unique<FakeEngine>();
  fake->throw_on_submit_at = 1;
  fake->throw_non_std = true;

  std::atomic<bool> g_running{true};
  fake->running = &g_running;
  WorkerThread worker(config, std::move(fake));

  std::barrier<> start_barrier{1};
  worker.Start(start_barrier, g_running);
  worker.Join();

  EXPECT_TRUE(worker.Failed());
  EXPECT_EQ(worker.ErrorMessage(), "unknown exception");
}

TEST(WorkerThreadTest, HealthyRunReportsNoFailure) {
  constexpr std::uint64_t kTarget = 20;
  auto config = MakeConfig("psync", RWMode::kRead, 1);

  auto fake = std::make_unique<FakeEngine>();
  fake->stop_after = kTarget;

  std::atomic<bool> g_running{true};
  fake->running = &g_running;
  WorkerThread worker(config, std::move(fake));

  std::barrier<> start_barrier{1};
  worker.Start(start_barrier, g_running);
  worker.Join();

  EXPECT_FALSE(worker.Failed());
  EXPECT_TRUE(worker.ErrorMessage().empty());
}

TEST(WorkerThreadTest, FailureStopsOtherWorkers) {
  auto failing_config = MakeConfig("psync", RWMode::kRead, 1);
  failing_config.name = "failing";
  auto healthy_config = MakeConfig("libaio", RWMode::kRandRead, 4);
  healthy_config.name = "healthy";

  std::atomic<bool> g_running{true};

  auto failing_engine = std::make_unique<FakeEngine>();
  failing_engine->throw_on_submit_at = 10;
  failing_engine->running = &g_running;

  auto healthy_engine = std::make_unique<FakeEngine>();
  healthy_engine->completions_per_poll = 4;
  healthy_engine->running = &g_running;
  FakeEngine* healthy_raw = healthy_engine.get();

  WorkerThread failing(failing_config, std::move(failing_engine));
  WorkerThread healthy(healthy_config, std::move(healthy_engine));

  std::barrier<> start_barrier{2};
  failing.Start(start_barrier, g_running);
  healthy.Start(start_barrier, g_running);
  failing.Join();
  healthy.Join();

  EXPECT_TRUE(failing.Failed());
  EXPECT_FALSE(healthy.Failed());
  EXPECT_FALSE(g_running.load(std::memory_order_relaxed));
  EXPECT_EQ(healthy_raw->in_flight, 0);
  EXPECT_EQ(healthy_raw->total_submitted, healthy_raw->total_completed);
}

TEST(WorkerThreadTest, ConcurrentRequestsUseDistinctBuffers) {
  constexpr int kDepth = 8;
  constexpr std::uint64_t kTarget = 200;
  auto config = MakeConfig("io_uring", RWMode::kRandRW, kDepth);

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

  EXPECT_FALSE(raw->duplicate_in_flight_buffer);
  EXPECT_EQ(raw->max_in_flight, kDepth);
  // Buffers are pooled and reused, so the pool never grows past iodepth.
  EXPECT_EQ(raw->distinct_buffers.size(), static_cast<size_t>(kDepth));
  EXPECT_GT(raw->total_submitted, static_cast<std::uint64_t>(kDepth));
}

TEST(WorkerThreadTest, SyncRunUsesOneBuffer) {
  constexpr std::uint64_t kTarget = 25;
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

  EXPECT_FALSE(raw->duplicate_in_flight_buffer);
  EXPECT_EQ(raw->distinct_buffers.size(), 1U);
}

TEST(WorkerThreadTest, SubmitFailureReleasesSlot) {
  constexpr int kDepth = 4;
  auto config = MakeConfig("libaio", RWMode::kRandRead, kDepth);

  auto fake = std::make_unique<FakeEngine>();
  fake->completions_per_poll = kDepth;
  fake->throw_on_submit_at = 3;
  FakeEngine* raw = fake.get();

  std::atomic<bool> g_running{true};
  fake->running = &g_running;
  WorkerThread worker(config, std::move(fake));

  std::barrier<> start_barrier{1};
  worker.Start(start_barrier, g_running);
  worker.Join();

  EXPECT_TRUE(worker.Failed());
  EXPECT_FALSE(raw->duplicate_in_flight_buffer);
  EXPECT_FALSE(g_running.load(std::memory_order_relaxed));
}

}  // namespace
}  // namespace cfio
