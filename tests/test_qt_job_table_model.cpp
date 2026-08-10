/// @file test_qt_job_table_model.cpp
/// @brief Unit tests for the Qt free live metrics table rows

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "display/qt/qt_job_table_model.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

constexpr std::uint64_t kMiB = 1024ULL * 1024ULL;
constexpr std::uint64_t kGiB = 1024ULL * kMiB;

std::size_t Index(JobTableColumn column) {
  return static_cast<std::size_t>(column);
}

MetricsSnapshot SampleSnapshot() {
  MetricsSnapshot snapshot;

  PerJobMetrics read;
  read.job_name = "rand-read-4k";
  read.iops_instant = 125432;
  read.iops_cumulative = 118200;
  read.bw_instant = 512 * kMiB;
  read.bw_cumulative = 483 * kMiB;
  read.lat_p50_ns = 8000;
  read.lat_p95_ns = 32000;
  read.lat_p99_ns = 45000;

  PerJobMetrics write;
  write.job_name = "seq-write-128k";
  write.iops_instant = 48291;
  write.iops_cumulative = 51003;
  write.bw_instant = 189 * kMiB;
  write.bw_cumulative = 198 * kMiB;
  write.lat_p50_ns = 22000;
  write.lat_p95_ns = 105000;
  write.lat_p99_ns = 312000;
  write.read_errors = 1;
  write.write_errors = 2;

  snapshot.jobs = {read, write};

  snapshot.aggregate.job_name = "TOTAL";
  snapshot.aggregate.iops_instant = 173723;
  snapshot.aggregate.iops_cumulative = 169203;
  snapshot.aggregate.bw_instant = 701 * kMiB;
  snapshot.aggregate.bw_cumulative = 681 * kMiB;
  snapshot.aggregate.lat_p50_ns = 777000;
  snapshot.aggregate.read_errors = 1;
  snapshot.aggregate.write_errors = 2;
  return snapshot;
}

TEST(JobTableHeadersTest, HasOneLabelPerColumn) {
  const std::vector<std::string> headers = JobTableHeaders();
  ASSERT_EQ(headers.size(), static_cast<std::size_t>(kJobTableColumnCount));
  EXPECT_EQ(headers[Index(JobTableColumn::kJob)], "Job");
  EXPECT_EQ(headers[Index(JobTableColumn::kIopsInstant)], "IOPS inst");
  EXPECT_EQ(headers[Index(JobTableColumn::kBwAverage)], "BW avg");
  EXPECT_EQ(headers[Index(JobTableColumn::kLatP99)], "P99");
  EXPECT_EQ(headers[Index(JobTableColumn::kErrors)], "Errors");
}

TEST(BuildJobTableRowsTest, EmitsOneRowPerJobPlusTotal) {
  const std::vector<JobTableRow> rows = BuildJobTableRows(SampleSnapshot());

  ASSERT_EQ(rows.size(), 3U);
  EXPECT_EQ(rows[0].cells[Index(JobTableColumn::kJob)], "rand-read-4k");
  EXPECT_EQ(rows[1].cells[Index(JobTableColumn::kJob)], "seq-write-128k");
  EXPECT_EQ(rows[2].cells[Index(JobTableColumn::kJob)], "TOTAL");
  EXPECT_FALSE(rows[0].is_total);
  EXPECT_FALSE(rows[1].is_total);
  EXPECT_TRUE(rows[2].is_total);
}

TEST(BuildJobTableRowsTest, FormatsEveryCellOfAJobRow) {
  const std::vector<JobTableRow> rows = BuildJobTableRows(SampleSnapshot());

  const JobTableRow& row = rows[0];
  ASSERT_EQ(row.cells.size(), static_cast<std::size_t>(kJobTableColumnCount));
  EXPECT_EQ(row.cells[Index(JobTableColumn::kIopsInstant)], "125,432");
  EXPECT_EQ(row.cells[Index(JobTableColumn::kIopsAverage)], "118,200");
  EXPECT_EQ(row.cells[Index(JobTableColumn::kBwInstant)], "512 MB/s");
  EXPECT_EQ(row.cells[Index(JobTableColumn::kBwAverage)], "483 MB/s");
  EXPECT_EQ(row.cells[Index(JobTableColumn::kLatP50)], "8 μs");
  EXPECT_EQ(row.cells[Index(JobTableColumn::kLatP95)], "32 μs");
  EXPECT_EQ(row.cells[Index(JobTableColumn::kLatP99)], "45 μs");
  EXPECT_EQ(row.cells[Index(JobTableColumn::kErrors)], "0");
  EXPECT_EQ(row.error_detail, "R:0 W:0");
}

TEST(BuildJobTableRowsTest, SumsErrorsAndKeepsTheSplitAsDetail) {
  const std::vector<JobTableRow> rows = BuildJobTableRows(SampleSnapshot());

  EXPECT_EQ(rows[1].cells[Index(JobTableColumn::kErrors)], "3");
  EXPECT_EQ(rows[1].error_detail, "R:1 W:2");
}

TEST(BuildJobTableRowsTest, TotalRowTakesTheAggregateAndDropsLatency) {
  const std::vector<JobTableRow> rows = BuildJobTableRows(SampleSnapshot());

  const JobTableRow& total = rows.back();
  EXPECT_EQ(total.cells[Index(JobTableColumn::kIopsInstant)], "173,723");
  EXPECT_EQ(total.cells[Index(JobTableColumn::kBwInstant)], "701 MB/s");
  EXPECT_EQ(total.cells[Index(JobTableColumn::kLatP50)], kJobTableNoValue);
  EXPECT_EQ(total.cells[Index(JobTableColumn::kLatP95)], kJobTableNoValue);
  EXPECT_EQ(total.cells[Index(JobTableColumn::kLatP99)], kJobTableNoValue);
  EXPECT_EQ(total.cells[Index(JobTableColumn::kErrors)], "3");
}

TEST(BuildJobTableRowsTest, EmptySnapshotYieldsOnlyTheTotalRow) {
  const std::vector<JobTableRow> rows = BuildJobTableRows(MetricsSnapshot{});

  ASSERT_EQ(rows.size(), 1U);
  EXPECT_TRUE(rows[0].is_total);
  EXPECT_EQ(rows[0].cells[Index(JobTableColumn::kJob)], "TOTAL");
  EXPECT_EQ(rows[0].cells[Index(JobTableColumn::kIopsInstant)], "0");
  EXPECT_EQ(rows[0].cells[Index(JobTableColumn::kBwInstant)], "0 MB/s");
}

TEST(BuildJobTableRowsTest, TruncatesSubUnitRatesAndLatency) {
  MetricsSnapshot snapshot;
  PerJobMetrics job;
  job.job_name = "tiny";
  job.bw_instant = 1024;
  job.lat_p99_ns = 999;
  snapshot.jobs.push_back(job);

  const std::vector<JobTableRow> rows = BuildJobTableRows(snapshot);

  EXPECT_EQ(rows[0].cells[Index(JobTableColumn::kBwInstant)], "0 MB/s");
  EXPECT_EQ(rows[0].cells[Index(JobTableColumn::kLatP99)], "0 μs");
}

BenchmarkResults SampleResults() {
  BenchmarkResults results;

  JobResults read;
  read.name = "rand-read-4k";
  read.iops_avg = 118200;
  read.bw_avg_bytes = 483 * kMiB;
  read.total_bytes = 20 * kGiB;

  JobResults write;
  write.name = "seq-write-128k";
  write.iops_avg = 51003;
  write.bw_avg_bytes = 198 * kMiB;
  write.total_bytes = 7 * kGiB;
  write.read_errors = 1;
  write.write_errors = 2;

  results.jobs = {read, write};
  return results;
}

TEST(BuildRunSummaryTest, SumsEveryJobAndMatchesTerminalFormatting) {
  EXPECT_EQ(BuildRunSummary(SampleResults()),
            "TOTAL   IOPS 169,203   BW 681 MB/s   IO 27.0 GiB   Err R:1 W:2");
}

TEST(BuildRunSummaryTest, MarksInterrupted) {
  BenchmarkResults results = SampleResults();
  results.interrupted = true;

  EXPECT_EQ(BuildRunSummary(results),
            "TOTAL   IOPS 169,203   BW 681 MB/s   IO 27.0 GiB   Err R:1 W:2 (interrupted)");
}

TEST(BuildRunSummaryTest, FailureOutranksInterrupted) {
  BenchmarkResults results = SampleResults();
  results.interrupted = true;
  results.jobs[1].failed = true;

  EXPECT_EQ(
      BuildRunSummary(results),
      "TOTAL   IOPS 169,203   BW 681 MB/s   IO 27.0 GiB   Err R:1 W:2 (aborted: job failure)");
}

TEST(BuildRunSummaryTest, EmptyResultsSumToZero) {
  EXPECT_EQ(BuildRunSummary(BenchmarkResults{}),
            "TOTAL   IOPS 0   BW 0 MB/s   IO 0 B   Err R:0 W:0");
}

}  // namespace
}  // namespace cfio
