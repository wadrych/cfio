/// @file test_config_validator.cpp
/// @brief Unit tests for ConfigValidator.

#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "config/config_validator.h"
#include "engine/engine_factory.h"

namespace cfio {
namespace {

// Dummy job config creator.
JobConfig MakeValidConfig(const std::string& name = "test-job") {
  JobConfig c;
  c.name = name;
  c.engine = "psync";
  c.rw_mode = RWMode::kRead;
  c.access_pattern = AccessPattern::kSequential;
  c.block_size = 4096;
  c.file_size = 1048576;  // 1 MiB
  c.iodepth = 1;
  c.direct = true;
  c.rwmixread = 50;
  c.filename = "./cfio-test.dat";
  c.alignment = 4096;
  return c;
}

// --- Happy path tests ---

TEST(ConfigValidatorTest, ValidConfig) {
  EXPECT_NO_THROW(ConfigValidator::Validate(MakeValidConfig()));
}

TEST(ConfigValidatorTest, ValidBoundarySizeEqualsBlockSize) {
  auto c = MakeValidConfig();
  c.block_size = 4096;
  c.file_size = 4096;
  EXPECT_NO_THROW(ConfigValidator::Validate(c));
}

TEST(ConfigValidatorTest, ValidBoundaryRWMixRead0) {
  auto c = MakeValidConfig();
  c.rwmixread = 0;
  EXPECT_NO_THROW(ConfigValidator::Validate(c));
}

TEST(ConfigValidatorTest, ValidBoundaryRWMixRead100) {
  auto c = MakeValidConfig();
  c.rwmixread = 100;
  EXPECT_NO_THROW(ConfigValidator::Validate(c));
}

TEST(ConfigValidatorTest, ValidBoundaryIODepth1) {
  auto c = MakeValidConfig();
  c.iodepth = 1;
  EXPECT_NO_THROW(ConfigValidator::Validate(c));
}

TEST(ConfigValidatorTest, ValidMultipleJobs) {
  std::vector<JobConfig> const configs = {MakeValidConfig("job-a"), MakeValidConfig("job-b")};
  EXPECT_NO_THROW(ConfigValidator::ValidateAll(configs));
}

// --- Alignment failures ---

TEST(ConfigValidatorTest, AlignmentZero) {
  auto c = MakeValidConfig();
  c.alignment = 0;
  EXPECT_THROW(ConfigValidator::Validate(c), std::runtime_error);
}

TEST(ConfigValidatorTest, AlignmentNotPowerOf2) {
  auto c = MakeValidConfig();
  c.alignment = 3000;
  EXPECT_THROW(ConfigValidator::Validate(c), std::runtime_error);
}

// --- Block size failures ---

TEST(ConfigValidatorTest, BlockSizeZero) {
  auto c = MakeValidConfig();
  c.block_size = 0;
  EXPECT_THROW(ConfigValidator::Validate(c), std::runtime_error);
}

TEST(ConfigValidatorTest, BlockSizeNotMultipleOfAlignment) {
  auto c = MakeValidConfig();
  c.block_size = 6144;  // 6k
  c.alignment = 4096;   // 4k
  c.file_size = 1048576;
  EXPECT_THROW(ConfigValidator::Validate(c), std::runtime_error);
}

// --- File size failures ---

TEST(ConfigValidatorTest, FileSizeLessThanBlockSize) {
  auto c = MakeValidConfig();
  c.block_size = 8192;
  c.file_size = 4096;
  EXPECT_THROW(ConfigValidator::Validate(c), std::runtime_error);
}

TEST(ConfigValidatorTest, FileSizeNotMultipleOfAlignment) {
  auto c = MakeValidConfig();
  c.file_size = 5000;
  c.alignment = 4096;
  c.block_size = 4096;
  EXPECT_THROW(ConfigValidator::Validate(c), std::runtime_error);
}

// --- Engine failures ---

TEST(ConfigValidatorTest, UnknownEngine) {
  auto c = MakeValidConfig();
  c.engine = "unknown";
  EXPECT_THROW(ConfigValidator::Validate(c), std::runtime_error);
}

// --- RWMixRead failures ---

TEST(ConfigValidatorTest, RWMixReadTooLow) {
  auto c = MakeValidConfig();
  c.rwmixread = -1;
  EXPECT_THROW(ConfigValidator::Validate(c), std::runtime_error);
}

TEST(ConfigValidatorTest, RWMixReadTooHigh) {
  auto c = MakeValidConfig();
  c.rwmixread = 101;
  EXPECT_THROW(ConfigValidator::Validate(c), std::runtime_error);
}

// --- IODepth failures ---

TEST(ConfigValidatorTest, IODepthZero) {
  auto c = MakeValidConfig();
  c.iodepth = 0;
  EXPECT_THROW(ConfigValidator::Validate(c), std::runtime_error);
}

// --- Name failures ---

TEST(ConfigValidatorTest, EmptyName) {
  auto c = MakeValidConfig();
  c.name = "";
  EXPECT_THROW(ConfigValidator::Validate(c), std::runtime_error);
}

// --- Multi-job validation (ValidateAll) ---

TEST(ConfigValidatorTest, DuplicateNames) {
  std::vector<JobConfig> const configs = {MakeValidConfig("same"), MakeValidConfig("same")};
  EXPECT_THROW(ConfigValidator::ValidateAll(configs), std::runtime_error);
}

TEST(ConfigValidatorTest, EmptyJobsVector) {
  std::vector<JobConfig> const configs;
  EXPECT_THROW(ConfigValidator::ValidateAll(configs), std::runtime_error);
}

// --- Engine name coverage ---

TEST(ConfigValidatorTest, AllKnownEnginesAccepted) {
  for (const auto& engine : EngineFactory::KnownEngines()) {
    auto c = MakeValidConfig();
    c.engine = engine;
    EXPECT_NO_THROW(ConfigValidator::Validate(c)) << "engine: " << engine;
  }
}

}  // namespace
}  // namespace cfio
