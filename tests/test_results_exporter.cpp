/// @file test_results_exporter.cpp
/// @brief Unit tests for ResultsExporter JSON summary and CSV time-series output.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "common/cli_options.h"
#include "common/types.h"
#include "config/job_config.h"
#include "reporting/results_exporter.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

JobResults MakeJobResults(const std::string& name, RWMode rw, std::uint64_t read_errors,
                          std::uint64_t write_errors, bool direct_effective) {
  JobConfig cfg;
  cfg.name = name;
  cfg.engine = "io_uring";
  cfg.rw_mode = rw;
  cfg.access_pattern = JobConfig::DeriveAccessPattern(rw);
  cfg.block_size = 4096;
  cfg.file_size = 1073741824;
  cfg.iodepth = 32;

  JobResults job;
  job.name = name;
  job.config = cfg;
  job.iops_avg = 118200;
  job.bw_avg_bytes = 484147200;
  job.lat_min_ns = 1200;
  job.lat_max_ns = 892000;
  job.lat_p50_ns = 8000;
  job.lat_p95_ns = 32000;
  job.lat_p99_ns = 45000;
  job.total_ios = 7092000;
  job.total_bytes = 29048832000ULL;
  job.read_errors = read_errors;
  job.write_errors = write_errors;
  job.direct_effective = direct_effective;
  return job;
}

constexpr std::string_view kCsvHeader =
    "timestamp_s,job_name,iops,bw_bytes,lat_p50_ns,lat_p95_ns,lat_p99_ns,errors";

PerJobMetrics MakePerJobMetrics(const std::string& name, std::uint64_t iops_instant,
                                std::uint64_t bw_instant, std::uint64_t read_errors,
                                std::uint64_t write_errors) {
  PerJobMetrics job;
  job.job_name = name;
  job.iops_instant = iops_instant;
  job.bw_instant = bw_instant;
  job.lat_p50_ns = 8000;
  job.lat_p95_ns = 32000;
  job.lat_p99_ns = 45000;
  job.read_errors = read_errors;
  job.write_errors = write_errors;
  return job;
}

MetricsSnapshot MakeSnapshot(std::vector<PerJobMetrics> jobs) {
  MetricsSnapshot snapshot;
  snapshot.jobs = std::move(jobs);
  return snapshot;
}

class ResultsExporterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    temp_dir_ = std::filesystem::path(::testing::TempDir()) / ("cfio_export_" + std::string(info->name()));
    std::filesystem::create_directories(temp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(temp_dir_); }

  nlohmann::json ReadSummary() const {
    std::ifstream in(temp_dir_ / "summary.json");
    nlohmann::json parsed;
    in >> parsed;
    return parsed;
  }

  std::vector<std::string> ReadTimeseriesLines() const {
    std::ifstream in(temp_dir_ / "timeseries.csv");
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
      lines.push_back(line);
    }
    return lines;
  }

  std::filesystem::path temp_dir_;
};

TEST_F(ResultsExporterTest, WritesTopLevelFields) {
  BenchmarkResults results;
  results.cfio_version = "0.1.0";
  results.timestamp = "2026-03-22T13:15:00Z";
  results.runtime_seconds = 60;
  results.jobs.push_back(MakeJobResults("rand-read-4k", RWMode::kRandRead, 0, 0, true));

  ResultsExporter::ExportJson(results, temp_dir_);

  ASSERT_TRUE(std::filesystem::exists(temp_dir_ / "summary.json"));
  const nlohmann::json root = ReadSummary();
  EXPECT_EQ(root.at("cfio_version"), "0.1.0");
  EXPECT_EQ(root.at("timestamp"), "2026-03-22T13:15:00Z");
  EXPECT_EQ(root.at("runtime_seconds"), 60);
  EXPECT_TRUE(root.at("jobs").is_array());
  EXPECT_EQ(root.at("jobs").size(), 1U);
}

TEST_F(ResultsExporterTest, GlobalConfigNullWhenNoOverrides) {
  BenchmarkResults results;
  results.runtime_seconds = 10;
  results.jobs.push_back(MakeJobResults("job", RWMode::kRead, 0, 0, true));

  ResultsExporter::ExportJson(results, temp_dir_);

  const nlohmann::json global = ReadSummary().at("global_config");
  EXPECT_TRUE(global.at("engine_override").is_null());
  EXPECT_TRUE(global.at("direct").is_null());
}

TEST_F(ResultsExporterTest, GlobalConfigSerializesOverrides) {
  BenchmarkResults results;
  results.runtime_seconds = 10;
  results.global_config.engine_override = "psync";
  results.global_config.direct_override = true;
  results.jobs.push_back(MakeJobResults("job", RWMode::kRead, 0, 0, true));

  ResultsExporter::ExportJson(results, temp_dir_);

  const nlohmann::json global = ReadSummary().at("global_config");
  EXPECT_EQ(global.at("engine_override"), "psync");
  EXPECT_EQ(global.at("direct"), true);
}

TEST_F(ResultsExporterTest, JobConfigSubsetIsSerialized) {
  BenchmarkResults results;
  results.jobs.push_back(MakeJobResults("rand-read-4k", RWMode::kRandRead, 0, 0, true));

  ResultsExporter::ExportJson(results, temp_dir_);

  const nlohmann::json config = ReadSummary().at("jobs").at(0).at("config");
  EXPECT_EQ(config.at("engine"), "io_uring");
  EXPECT_EQ(config.at("rw"), "randread");
  EXPECT_EQ(config.at("bs"), 4096);
  EXPECT_EQ(config.at("size"), 1073741824);
  EXPECT_EQ(config.at("iodepth"), 32);
  EXPECT_EQ(config.size(), 5U);
}

TEST_F(ResultsExporterTest, JobResultsFieldsAreSerialized) {
  BenchmarkResults results;
  results.jobs.push_back(MakeJobResults("rand-read-4k", RWMode::kRandRead, 0, 0, true));

  ResultsExporter::ExportJson(results, temp_dir_);

  const nlohmann::json res = ReadSummary().at("jobs").at(0).at("results");
  EXPECT_EQ(res.at("iops_avg"), 118200);
  EXPECT_EQ(res.at("bw_avg_bytes"), 484147200);
  EXPECT_EQ(res.at("lat_min_ns"), 1200);
  EXPECT_EQ(res.at("lat_max_ns"), 892000);
  EXPECT_EQ(res.at("lat_p50_ns"), 8000);
  EXPECT_EQ(res.at("lat_p95_ns"), 32000);
  EXPECT_EQ(res.at("lat_p99_ns"), 45000);
  EXPECT_EQ(res.at("total_ios"), 7092000);
  EXPECT_EQ(res.at("total_bytes"), 29048832000ULL);
}

TEST_F(ResultsExporterTest, ErrorsAreSplitReadWrite) {
  BenchmarkResults results;
  results.jobs.push_back(MakeJobResults("job", RWMode::kRandRW, 3, 7, true));

  ResultsExporter::ExportJson(results, temp_dir_);

  const nlohmann::json errors = ReadSummary().at("jobs").at(0).at("results").at("errors");
  EXPECT_EQ(errors.at("read"), 3);
  EXPECT_EQ(errors.at("write"), 7);
}

TEST_F(ResultsExporterTest, SerializesMultipleJobs) {
  BenchmarkResults results;
  results.jobs.push_back(MakeJobResults("rand-read-4k", RWMode::kRandRead, 0, 0, true));
  results.jobs.push_back(MakeJobResults("seq-write-128k", RWMode::kWrite, 0, 1, false));

  ResultsExporter::ExportJson(results, temp_dir_);

  const nlohmann::json jobs = ReadSummary().at("jobs");
  ASSERT_EQ(jobs.size(), 2U);
  EXPECT_EQ(jobs.at(0).at("name"), "rand-read-4k");
  EXPECT_EQ(jobs.at(1).at("name"), "seq-write-128k");
  EXPECT_EQ(jobs.at(1).at("config").at("rw"), "write");
}

TEST_F(ResultsExporterTest, ThrowsWhenOutputDirMissing) {
  BenchmarkResults results;
  results.jobs.push_back(MakeJobResults("job", RWMode::kRead, 0, 0, true));

  const std::filesystem::path missing = temp_dir_ / "does" / "not" / "exist";
  EXPECT_THROW(ResultsExporter::ExportJson(results, missing), std::runtime_error);
}

TEST_F(ResultsExporterTest, WritesTimeseriesHeader) {
  std::vector<MetricsSnapshot> series;
  series.push_back(MakeSnapshot({MakePerJobMetrics("rand-read-4k", 125432, 513769472, 0, 0)}));

  ResultsExporter::ExportCsv(series, temp_dir_);

  ASSERT_TRUE(std::filesystem::exists(temp_dir_ / "timeseries.csv"));
  const std::vector<std::string> lines = ReadTimeseriesLines();
  ASSERT_FALSE(lines.empty());
  EXPECT_EQ(lines.front(), std::string(kCsvHeader));
}

TEST_F(ResultsExporterTest, WritesOneRowPerJobPerSecond) {
  const std::vector<PerJobMetrics> jobs = {
      MakePerJobMetrics("rand-read-4k", 125432, 513769472, 0, 0),
      MakePerJobMetrics("seq-write-128k", 48291, 198066176, 0, 0)};
  std::vector<MetricsSnapshot> series;
  series.push_back(MakeSnapshot(jobs));
  series.push_back(MakeSnapshot(jobs));

  ResultsExporter::ExportCsv(series, temp_dir_);

  const std::vector<std::string> lines = ReadTimeseriesLines();
  ASSERT_EQ(lines.size(), 5U);
  EXPECT_TRUE(lines[1].starts_with("1,rand-read-4k,"));
  EXPECT_TRUE(lines[2].starts_with("1,seq-write-128k,"));
  EXPECT_TRUE(lines[3].starts_with("2,rand-read-4k,"));
  EXPECT_TRUE(lines[4].starts_with("2,seq-write-128k,"));
}

TEST_F(ResultsExporterTest, MapsInstantaneousFieldsAndCombinedErrors) {
  std::vector<MetricsSnapshot> series;
  series.push_back(MakeSnapshot({MakePerJobMetrics("job", 1000, 4096000, 3, 7)}));

  ResultsExporter::ExportCsv(series, temp_dir_);

  const std::vector<std::string> lines = ReadTimeseriesLines();
  ASSERT_EQ(lines.size(), 2U);
  EXPECT_EQ(lines[1], "1,job,1000,4096000,8000,32000,45000,10");
}

TEST_F(ResultsExporterTest, EmptyTimeSeriesWritesHeaderOnly) {
  const std::vector<MetricsSnapshot> series;

  ResultsExporter::ExportCsv(series, temp_dir_);

  const std::vector<std::string> lines = ReadTimeseriesLines();
  ASSERT_EQ(lines.size(), 1U);
  EXPECT_EQ(lines.front(), std::string(kCsvHeader));
}

TEST_F(ResultsExporterTest, CsvThrowsWhenOutputDirMissing) {
  std::vector<MetricsSnapshot> series;
  series.push_back(MakeSnapshot({MakePerJobMetrics("job", 1, 1, 0, 0)}));

  const std::filesystem::path missing = temp_dir_ / "does" / "not" / "exist";
  EXPECT_THROW(ResultsExporter::ExportCsv(series, missing), std::runtime_error);
}

}  // namespace
}  // namespace cfio
