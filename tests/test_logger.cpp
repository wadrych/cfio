/// @file test_logger.cpp
/// @brief Unit tests for Logger

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <spdlog/common.h>

#include "logging/logger.h"

namespace cfio {
namespace {

/// @brief Fixture that always leaves the logger initialized.
///
class LoggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    temp_dir_ = std::filesystem::path(::testing::TempDir()) / ("cfio_" + std::string(info->name()));
    std::filesystem::create_directories(temp_dir_);
  }

  void TearDown() override {
    Logger::Shutdown();
    std::filesystem::remove_all(temp_dir_);
    Logger::Init(std::filesystem::path(::testing::TempDir()) / "cfio_test_restore.log",
                 /*verbose=*/false);
  }

  [[nodiscard]] std::filesystem::path LogPath() const { return temp_dir_ / "cfio.log"; }

  std::filesystem::path temp_dir_;
};

TEST_F(LoggerTest, GetReturnsLoggerAfterInit) {
  auto logger = Logger::Get();
  ASSERT_NE(logger, nullptr);
  EXPECT_EQ(logger->name(), "cfio");
}

TEST_F(LoggerTest, DoubleInitThrows) {
  EXPECT_THROW(Logger::Init(LogPath(), /*verbose=*/false), std::runtime_error);
}

TEST_F(LoggerTest, GetBeforeInitThrows) {
  Logger::Shutdown();
  EXPECT_THROW(Logger::Get(), std::runtime_error);
}

TEST_F(LoggerTest, ShutdownIsIdempotent) {
  Logger::Shutdown();
  EXPECT_NO_THROW(Logger::Shutdown());
  EXPECT_NO_THROW(Logger::Shutdown());
}

TEST_F(LoggerTest, InitAfterShutdownSucceeds) {
  Logger::Shutdown();
  ASSERT_NO_THROW(Logger::Init(LogPath(), /*verbose=*/false));
  EXPECT_NE(Logger::Get(), nullptr);
}

TEST_F(LoggerTest, InitCreatesLogFile) {
  Logger::Shutdown();
  Logger::Init(LogPath(), /*verbose=*/false);
  ASSERT_TRUE(std::filesystem::exists(LogPath()));

  Logger::Get()->info("probe message");

  Logger::Shutdown();

  EXPECT_GT(std::filesystem::file_size(LogPath()), 0U);
}

TEST_F(LoggerTest, VerboseSelectsDebugLevel) {
  Logger::Shutdown();
  Logger::Init(LogPath(), /*verbose=*/true);
  EXPECT_EQ(Logger::Get()->level(), spdlog::level::debug);
}

TEST_F(LoggerTest, NonVerboseSelectsInfoLevel) {
  Logger::Shutdown();
  Logger::Init(LogPath(), /*verbose=*/false);
  EXPECT_EQ(Logger::Get()->level(), spdlog::level::info);
}

TEST_F(LoggerTest, InitTruncatesExistingFile) {
  {
    std::ofstream out(LogPath());
    out << "stale content from a previous run\n";
  }
  ASSERT_GT(std::filesystem::file_size(LogPath()), 0U);

  Logger::Shutdown();
  Logger::Init(LogPath(), /*verbose=*/false);

  EXPECT_EQ(std::filesystem::file_size(LogPath()), 0U);
}

}  // namespace
}  // namespace cfio
