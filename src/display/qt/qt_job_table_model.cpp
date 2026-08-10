/// @file qt_job_table_model.cpp
/// @brief Implementation of the Qt free live metrics table rows

#include "display/qt/qt_job_table_model.h"

#include <string>
#include <vector>

#include <fmt/format.h>

#include "display/metric_format.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

constexpr const char* kTotalRowName = "TOTAL";

JobTableRow MakeRow(const PerJobMetrics& job, bool is_total) {
  JobTableRow row;
  row.is_total = is_total;
  row.cells.reserve(kJobTableColumnCount);
  row.cells.push_back(is_total ? kTotalRowName : job.job_name);
  row.cells.push_back(FormatCount(job.iops_instant));
  row.cells.push_back(FormatCount(job.iops_cumulative));
  row.cells.push_back(FormatRate(job.bw_instant));
  row.cells.push_back(FormatRate(job.bw_cumulative));
  if (is_total) {
    row.cells.emplace_back(kJobTableNoValue);
    row.cells.emplace_back(kJobTableNoValue);
    row.cells.emplace_back(kJobTableNoValue);
  } else {
    row.cells.push_back(FormatLatencyUs(job.lat_p50_ns));
    row.cells.push_back(FormatLatencyUs(job.lat_p95_ns));
    row.cells.push_back(FormatLatencyUs(job.lat_p99_ns));
  }
  row.cells.push_back(FormatCount(job.read_errors + job.write_errors));
  row.error_detail = fmt::format("R:{} W:{}", job.read_errors, job.write_errors);
  return row;
}

}  // namespace

std::vector<std::string> JobTableHeaders() {
  return {"Job", "IOPS inst", "IOPS avg", "BW inst", "BW avg", "P50", "P95", "P99", "Errors"};
}

std::vector<JobTableRow> BuildJobTableRows(const MetricsSnapshot& snapshot) {
  std::vector<JobTableRow> rows;
  rows.reserve(snapshot.jobs.size() + 1);
  for (const PerJobMetrics& job : snapshot.jobs) {
    rows.push_back(MakeRow(job, false));
  }
  rows.push_back(MakeRow(snapshot.aggregate, true));
  return rows;
}

}  // namespace cfio
