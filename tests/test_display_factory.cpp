/// @file test_display_factory.cpp
/// @brief Unit tests for IDisplay and DisplayFactory

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "display/display_factory.h"
#include "display/i_display.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

class FakeDisplay : public IDisplay {
 public:
  void Init(int runtime_seconds) override {
    calls.emplace_back("init");
    init_runtime = runtime_seconds;
  }

  void Update(const MetricsSnapshot& snapshot) override {
    calls.emplace_back("update");
    update_jobs = snapshot.jobs.size();
  }

  void ShowSummary(const BenchmarkResults& results) override {
    calls.emplace_back("summary");
    summary_runtime = results.runtime_seconds;
  }

  void Shutdown() override { calls.emplace_back("shutdown"); }

  std::vector<std::string> calls;
  int init_runtime{};
  std::size_t update_jobs{};
  int summary_runtime{};
};

TEST(DisplayFactoryTest, TuiBackendThrowsWhenNotCompiled) {
  EXPECT_THROW(DisplayFactory::Create("tui"), std::runtime_error);
}

TEST(DisplayFactoryTest, QtBackendThrowsWhenNotCompiled) {
  EXPECT_THROW(DisplayFactory::Create("qt"), std::runtime_error);
}

TEST(DisplayFactoryTest, UnknownBackendThrows) {
  EXPECT_THROW(DisplayFactory::Create("bogus"), std::runtime_error);
}

TEST(DisplayTest, FakeDisplayRecordsLifecycle) {
  FakeDisplay display;
  MetricsSnapshot snapshot;
  snapshot.jobs.resize(2);
  BenchmarkResults results;
  results.runtime_seconds = 30;

  display.Init(results.runtime_seconds);
  display.Update(snapshot);
  display.ShowSummary(results);
  display.Shutdown();

  const std::vector<std::string> expected = {"init", "update", "summary", "shutdown"};
  EXPECT_EQ(display.calls, expected);
  EXPECT_EQ(display.init_runtime, 30);
  EXPECT_EQ(display.update_jobs, 2U);
  EXPECT_EQ(display.summary_runtime, 30);
}

}  // namespace
}  // namespace cfio
