/// @file test_main.cpp
/// @brief Test entry point and process wide logger environment.

#include <filesystem>

#include <gtest/gtest.h>

#include "logging/logger.h"

namespace cfio {

/// @brief Keeps the logger initialized for the whole test run.
///
class LoggerEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    auto log_path = std::filesystem::path{::testing::TempDir()} / "cfio_test.log";
    Logger::Init(log_path, /*verbose=*/false);
  }

  void TearDown() override { Logger::Shutdown(); }
};

}  // namespace cfio

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::AddGlobalTestEnvironment(new cfio::LoggerEnvironment);
  return RUN_ALL_TESTS();
}
