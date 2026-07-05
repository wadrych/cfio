/// @file benchmark_orchestrator.cpp
/// @brief BenchmarkOrchestrator implementation

#include "orchestrator/benchmark_orchestrator.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "config/job_config.h"
#include "display/display_factory.h"
#include "engine/engine_factory.h"
#include "engine/i_engine_io.h"
#include "logging/logger.h"
#include "reporting/results_exporter.h"

namespace cfio {
namespace {

std::atomic<bool>* g_run_flag = nullptr;
static_assert(std::atomic<bool>::is_always_lock_free, "signal handler needs a lock free flag");

/// @brief Stop the active run.
extern "C" void OnSignal(int /*signum*/) {
  if (g_run_flag != nullptr) {
    g_run_flag->store(false, std::memory_order_relaxed);
  }
}

/// @brief Installs SIGINT and SIGTERM handlers
class SignalScope {
 public:
  /// @param flag  Run flag the handler clears
  explicit SignalScope(std::atomic<bool>* flag) {
    g_run_flag = flag;
    struct sigaction action {};
    action.sa_handler = OnSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, &old_int_);
    sigaction(SIGTERM, &action, &old_term_);
  }

  ~SignalScope() {
    sigaction(SIGINT, &old_int_, nullptr);
    sigaction(SIGTERM, &old_term_, nullptr);
    g_run_flag = nullptr;
  }

  SignalScope(const SignalScope&) = delete;
  SignalScope& operator=(const SignalScope&) = delete;
  SignalScope(SignalScope&&) = delete;
  SignalScope& operator=(SignalScope&&) = delete;

 private:
  struct sigaction old_int_ {};
  struct sigaction old_term_ {};
};

/// @brief Shuts down the display.
class DisplayScope {
 public:
  explicit DisplayScope(IDisplay* display) : display_(display) {}

  ~DisplayScope() {
    if (display_ != nullptr) {
      display_->Shutdown();
    }
  }

  DisplayScope(const DisplayScope&) = delete;
  DisplayScope& operator=(const DisplayScope&) = delete;
  DisplayScope(DisplayScope&&) = delete;
  DisplayScope& operator=(DisplayScope&&) = delete;

 private:
  IDisplay* display_;
};

/// @brief Local time for output directory names.
std::string LocalTimestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_r(&now, &local);
  std::array<char, 32> buffer{};
  const std::size_t written = std::strftime(buffer.data(), buffer.size(), "%Y%m%dT%H%M%S", &local);
  return {buffer.data(), written};
}

/// @brief Engine name for the display header.
std::string DeriveEngineLabel(const CliOptions& options, const std::vector<JobConfig>& jobs) {
  if (options.engine_override.has_value()) {
    return options.engine_override.value();
  }
  const std::string& first = jobs.front().engine;
  for (const JobConfig& job : jobs) {
    if (job.engine != first) {
      return "mixed";
    }
  }
  return first;
}

/// @brief Direct flag for the display header.
std::string DeriveDirectLabel(const CliOptions& options, const std::vector<JobConfig>& jobs) {
  if (options.direct_override.has_value()) {
    return options.direct_override.value() ? "ON" : "OFF";
  }
  const bool first = jobs.front().direct;
  for (const JobConfig& job : jobs) {
    if (job.direct != first) {
      return "mixed";
    }
  }
  return first ? "ON" : "OFF";
}

}  // namespace

BenchmarkOrchestrator::BenchmarkOrchestrator(CliOptions options, std::vector<JobConfig> jobs)
    : options_(std::move(options)), jobs_(std::move(jobs)), file_prep_(options_.keep_files) {
}

int BenchmarkOrchestrator::Run() {
  try {
    SetupOutputDirectory();
    InitLogging();

    auto log = Logger::get();
    log->info("starting benchmark: {} job(s), runtime {}s", jobs_.size(), options_.runtime_seconds);

    PrecreateFiles();
    CreateWorkers();

    BenchmarkResults results;
    RunBenchmark(results);
    ExportResults(results);
    CleanupFiles();

    log->info("benchmark complete, results in '{}'", output_dir_.string());
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "C-FIO: error: " << e.what() << "\n";
    if (logging_ready_) {
      Logger::get()->critical("fatal: {}", e.what());
    }
    return EXIT_FAILURE;
  }
}

void BenchmarkOrchestrator::SetupOutputDirectory() {
  if (options_.output_dir.empty()) {
    const std::string dir_name = jobs_.front().name + "-" + LocalTimestamp();
    output_dir_ = std::filesystem::path("cfio-results") / dir_name;
  } else {
    output_dir_ = options_.output_dir;
  }
  std::filesystem::create_directories(output_dir_);
}

void BenchmarkOrchestrator::InitLogging() {
  Logger::shutdown();
  Logger::init(output_dir_ / "cfio.log", options_.verbose);
  logging_ready_ = true;
}

void BenchmarkOrchestrator::PrecreateFiles() {
  auto log = Logger::get();
  for (const JobConfig& job : jobs_) {
    log->info("preparing '{}' for job '{}' ({} bytes)", job.filename.string(), job.name,
              job.file_size);
    file_prep_.CreateAndFill(job);
  }
}

void BenchmarkOrchestrator::CreateWorkers() {
  auto log = Logger::get();
  workers_.reserve(jobs_.size());
  for (const JobConfig& job : jobs_) {
    std::unique_ptr<IEngineIO> engine = EngineFactory::Create(job.engine);
    workers_.push_back(std::make_unique<WorkerThread>(job, std::move(engine)));
    log->info("worker ready for job '{}' engine '{}'", job.name, job.engine);
  }
}

void BenchmarkOrchestrator::RunBenchmark(BenchmarkResults& results) {
  auto log = Logger::get();

  display_ = DisplayFactory::Create(options_.ui_backend, BuildDisplayContext());
  const DisplayScope display_scope(display_.get());
  display_->Init(options_.runtime_seconds);

  std::vector<WorkerThread*> worker_ptrs;
  worker_ptrs.reserve(workers_.size());
  for (const std::unique_ptr<WorkerThread>& worker : workers_) {
    worker_ptrs.push_back(worker.get());
  }
  aggregator_ =
      std::make_unique<MetricsAggregator>(std::move(worker_ptrs), options_.runtime_seconds);
  aggregator_->Start(g_running_);

  const SignalScope signal_scope(&g_running_);

  std::barrier<> start_barrier(static_cast<std::ptrdiff_t>(workers_.size()));
  for (const std::unique_ptr<WorkerThread>& worker : workers_) {
    worker->Start(start_barrier, g_running_);
  }
  log->info("launched {} worker(s)", workers_.size());

  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(options_.runtime_seconds);
  auto next_tick = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (g_running_.load(std::memory_order_relaxed) &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_until(std::min(next_tick, deadline));
    if (!g_running_.load(std::memory_order_relaxed)) {
      break;  // interrupted by signal
    }
    display_->Update(aggregator_->LatestSnapshot());
    next_tick += std::chrono::seconds(1);
  }
  g_running_.store(false, std::memory_order_relaxed);
  log->info("run finished, stopping workers");

  for (const std::unique_ptr<WorkerThread>& worker : workers_) {
    worker->Join();
  }

  aggregator_->TakeFinalSnapshot();
  aggregator_->Stop();

  results = aggregator_->BuildResults(options_);
  display_->ShowSummary(results);
}

void BenchmarkOrchestrator::ExportResults(const BenchmarkResults& results) {
  ResultsExporter::ExportJson(results, output_dir_);
  ResultsExporter::ExportCsv(results.time_series, output_dir_);
}

void BenchmarkOrchestrator::CleanupFiles() {
  file_prep_.Cleanup();
}

DisplayContext BenchmarkOrchestrator::BuildDisplayContext() const {
  DisplayContext context;
  context.engine_label = DeriveEngineLabel(options_, jobs_);
  context.direct_label = DeriveDirectLabel(options_, jobs_);
  context.log_path = output_dir_ / "cfio.log";
  return context;
}

}  // namespace cfio
