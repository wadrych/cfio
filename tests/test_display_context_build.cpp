/// @file test_display_context_build.cpp
/// @brief Unit tests for the display header metadata builder

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "common/cli_options.h"
#include "config/job_config.h"
#include "display/display_context.h"
#include "display/display_context_build.h"

namespace cfio {
namespace {

JobConfig MakeJob(const std::string& name, const std::string& engine, bool direct) {
  JobConfig job;
  job.name = name;
  job.engine = engine;
  job.direct = direct;
  return job;
}

TEST(DisplayContextBuildTest, SingleJobLabels) {
  const CliOptions options;
  const std::vector<JobConfig> jobs = {MakeJob("j1", "io_uring", true)};

  EXPECT_EQ(DeriveEngineLabel(options, jobs), "io_uring");
  EXPECT_EQ(DeriveDirectLabel(options, jobs), "ON");
}

TEST(DisplayContextBuildTest, AgreeingJobsShareLabels) {
  const CliOptions options;
  const std::vector<JobConfig> jobs = {MakeJob("j1", "psync", false),
                                       MakeJob("j2", "psync", false)};

  EXPECT_EQ(DeriveEngineLabel(options, jobs), "psync");
  EXPECT_EQ(DeriveDirectLabel(options, jobs), "OFF");
}

TEST(DisplayContextBuildTest, DisagreeingJobsReportMixed) {
  const CliOptions options;
  const std::vector<JobConfig> jobs = {MakeJob("j1", "psync", true),
                                       MakeJob("j2", "libaio", false)};

  EXPECT_EQ(DeriveEngineLabel(options, jobs), "mixed");
  EXPECT_EQ(DeriveDirectLabel(options, jobs), "mixed");
}

TEST(DisplayContextBuildTest, OverridesWinOverJobs) {
  CliOptions options;
  options.engine_override = "sync";
  options.direct_override = false;
  const std::vector<JobConfig> jobs = {MakeJob("j1", "psync", true), MakeJob("j2", "libaio", true)};

  EXPECT_EQ(DeriveEngineLabel(options, jobs), "sync");
  EXPECT_EQ(DeriveDirectLabel(options, jobs), "OFF");
}

TEST(DisplayContextBuildTest, EmptyJobsAreSafe) {
  const CliOptions options;
  const std::vector<JobConfig> jobs;

  EXPECT_TRUE(DeriveEngineLabel(options, jobs).empty());
  EXPECT_EQ(DeriveDirectLabel(options, jobs), "OFF");
}

TEST(DisplayContextBuildTest, MakeContextCarriesLogPath) {
  const CliOptions options;
  const std::vector<JobConfig> jobs = {MakeJob("j1", "psync", true)};

  const DisplayContext context = MakeDisplayContext(options, jobs, "cfio-results/j1/cfio.log");

  EXPECT_EQ(context.engine_label, "psync");
  EXPECT_EQ(context.direct_label, "ON");
  EXPECT_EQ(context.log_path, "cfio-results/j1/cfio.log");
}

TEST(DisplayContextBuildTest, MakeContextAcceptsEmptyLogPath) {
  const CliOptions options;
  const std::vector<JobConfig> jobs = {MakeJob("j1", "psync", true)};

  EXPECT_TRUE(MakeDisplayContext(options, jobs, {}).log_path.empty());
}

}  // namespace
}  // namespace cfio
