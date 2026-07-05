/// @file test_benchmark_orchestrator.cpp
/// @brief Integration test for BenchmarkOrchestrator

#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <system_error>
#include <thread>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "common/cli_options.h"
#include "common/types.h"
#include "config/job_config.h"
#include "logging/logger.h"
#include "orchestrator/benchmark_orchestrator.h"

namespace cfio {
namespace {

constexpr size_t kBlockSize = 4096;
constexpr size_t kFileSize = 1U << 20;  // 1 MiB
constexpr int kRuntimeSeconds = 2;

class BenchmarkOrchestratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    base_dir_ =
        std::filesystem::path(::testing::TempDir()) / ("cfio_orch_" + std::string(info->name()));
    data_dir_ = base_dir_ / "data";
    out_dir_ = base_dir_ / "out";
    std::filesystem::create_directories(data_dir_);
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(base_dir_, ec);
  }

  JobConfig MakeJob() const {
    JobConfig job;
    job.name = "smoke";
    job.engine = "psync";
    job.rw_mode = RWMode::kRead;
    job.access_pattern = AccessPattern::kSequential;
    job.block_size = kBlockSize;
    job.file_size = kFileSize;
    job.iodepth = 1;
    job.direct = false;
    job.rwmixread = 100;
    job.filename = data_dir_ / "smoke.dat";
    job.alignment = kBlockSize;
    return job;
  }

  CliOptions MakeOptions() const {
    CliOptions opts;
    opts.runtime_seconds = kRuntimeSeconds;
    opts.output_dir = out_dir_;
    opts.ui_backend = "terminal";
    opts.keep_files = false;
    return opts;
  }

  std::filesystem::path base_dir_;
  std::filesystem::path data_dir_;
  std::filesystem::path out_dir_;
};

TEST_F(BenchmarkOrchestratorTest, RunsFullLifecycleAndWritesOutputs) {
  const JobConfig job = MakeJob();
  const CliOptions opts = MakeOptions();

  int exit_code = EXIT_FAILURE;
  {
    std::ostringstream sink;
    std::streambuf* saved = std::cout.rdbuf(sink.rdbuf());
    BenchmarkOrchestrator orchestrator(opts, {job});
    exit_code = orchestrator.Run();
    std::cout.rdbuf(saved);
  }
  EXPECT_EQ(exit_code, EXIT_SUCCESS);

  Logger::get()->flush();

  const std::filesystem::path summary = out_dir_ / "summary.json";
  const std::filesystem::path timeseries = out_dir_ / "timeseries.csv";
  const std::filesystem::path log = out_dir_ / "cfio.log";

  ASSERT_TRUE(std::filesystem::exists(summary));
  ASSERT_TRUE(std::filesystem::exists(timeseries));
  ASSERT_TRUE(std::filesystem::exists(log));
  EXPECT_GT(std::filesystem::file_size(summary), 0U);
  EXPECT_GT(std::filesystem::file_size(timeseries), 0U);

  EXPECT_FALSE(std::filesystem::exists(job.filename));

  std::ifstream in(summary);
  const nlohmann::json root = nlohmann::json::parse(in);
  ASSERT_TRUE(root.contains("jobs"));
  ASSERT_EQ(root["jobs"].size(), 1U);
  EXPECT_EQ(root["jobs"][0]["name"], "smoke");
  EXPECT_GT(root["jobs"][0]["results"]["total_ios"].get<std::uint64_t>(), 0U);
}

TEST_F(BenchmarkOrchestratorTest, SignalStopsRunEarly) {
  const JobConfig job = MakeJob();
  CliOptions opts = MakeOptions();
  opts.runtime_seconds = 30;

  std::thread raiser([]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    std::raise(SIGINT);
  });

  int exit_code = EXIT_FAILURE;
  const auto begin = std::chrono::steady_clock::now();
  {
    std::ostringstream sink;
    std::streambuf* saved = std::cout.rdbuf(sink.rdbuf());
    BenchmarkOrchestrator orchestrator(opts, {job});
    exit_code = orchestrator.Run();
    std::cout.rdbuf(saved);
  }
  const auto elapsed = std::chrono::steady_clock::now() - begin;
  raiser.join();

  EXPECT_EQ(exit_code, EXIT_SUCCESS);
  EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(), 10);

  Logger::get()->flush();
  EXPECT_TRUE(std::filesystem::exists(out_dir_ / "summary.json"));
}

}  // namespace
}  // namespace cfio
