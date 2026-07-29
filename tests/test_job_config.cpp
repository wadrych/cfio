/// @file test_job_config.cpp
/// @brief Unit tests for JobConfig helper methods.

#include <array>
#include <stdexcept>

#include <gtest/gtest.h>

#include "config/job_config.h"

namespace cfio {
namespace {

// --- ParseRWMode: valid strings ---

TEST(JobConfigParseRWMode, Read) {
  EXPECT_EQ(JobConfig::ParseRWMode("read"), RWMode::kRead);
}

TEST(JobConfigParseRWMode, Write) {
  EXPECT_EQ(JobConfig::ParseRWMode("write"), RWMode::kWrite);
}

TEST(JobConfigParseRWMode, RandRead) {
  EXPECT_EQ(JobConfig::ParseRWMode("randread"), RWMode::kRandRead);
}

TEST(JobConfigParseRWMode, RandWrite) {
  EXPECT_EQ(JobConfig::ParseRWMode("randwrite"), RWMode::kRandWrite);
}

TEST(JobConfigParseRWMode, ReadWrite) {
  EXPECT_EQ(JobConfig::ParseRWMode("readwrite"), RWMode::kReadWrite);
}

TEST(JobConfigParseRWMode, RandRW) {
  EXPECT_EQ(JobConfig::ParseRWMode("randrw"), RWMode::kRandRW);
}

// --- ParseRWMode: invalid strings ---

TEST(JobConfigParseRWMode, UnknownThrows) {
  EXPECT_THROW(JobConfig::ParseRWMode("unknown"), std::invalid_argument);
}

TEST(JobConfigParseRWMode, EmptyThrows) {
  EXPECT_THROW(JobConfig::ParseRWMode(""), std::invalid_argument);
}

// --- DeriveAccessPattern ---

TEST(JobConfigDerivePattern, ReadIsSequential) {
  EXPECT_EQ(JobConfig::DeriveAccessPattern(RWMode::kRead), AccessPattern::kSequential);
}

TEST(JobConfigDerivePattern, WriteIsSequential) {
  EXPECT_EQ(JobConfig::DeriveAccessPattern(RWMode::kWrite), AccessPattern::kSequential);
}

TEST(JobConfigDerivePattern, ReadWriteIsSequential) {
  EXPECT_EQ(JobConfig::DeriveAccessPattern(RWMode::kReadWrite), AccessPattern::kSequential);
}

TEST(JobConfigDerivePattern, RandReadIsRandom) {
  EXPECT_EQ(JobConfig::DeriveAccessPattern(RWMode::kRandRead), AccessPattern::kRandom);
}

TEST(JobConfigDerivePattern, RandWriteIsRandom) {
  EXPECT_EQ(JobConfig::DeriveAccessPattern(RWMode::kRandWrite), AccessPattern::kRandom);
}

TEST(JobConfigDerivePattern, RandRWIsRandom) {
  EXPECT_EQ(JobConfig::DeriveAccessPattern(RWMode::kRandRW), AccessPattern::kRandom);
}

// --- ToString: RWMode ---

TEST(JobConfigToString, RWModeRead) {
  EXPECT_EQ(JobConfig::ToString(RWMode::kRead), "read");
}

TEST(JobConfigToString, RWModeWrite) {
  EXPECT_EQ(JobConfig::ToString(RWMode::kWrite), "write");
}

TEST(JobConfigToString, RWModeRandRead) {
  EXPECT_EQ(JobConfig::ToString(RWMode::kRandRead), "randread");
}

TEST(JobConfigToString, RWModeRandWrite) {
  EXPECT_EQ(JobConfig::ToString(RWMode::kRandWrite), "randwrite");
}

TEST(JobConfigToString, RWModeReadWrite) {
  EXPECT_EQ(JobConfig::ToString(RWMode::kReadWrite), "readwrite");
}

TEST(JobConfigToString, RWModeRandRW) {
  EXPECT_EQ(JobConfig::ToString(RWMode::kRandRW), "randrw");
}

// --- ToString: AccessPattern ---

TEST(JobConfigToString, AccessPatternSequential) {
  EXPECT_EQ(JobConfig::ToString(AccessPattern::kSequential), "sequential");
}

TEST(JobConfigToString, AccessPatternRandom) {
  EXPECT_EQ(JobConfig::ToString(AccessPattern::kRandom), "random");
}

// --- ToString / ParseRWMode ---

TEST(JobConfigToString, RoundtripThroughParseRWMode) {
  constexpr std::array<RWMode, 6> kAllModes{RWMode::kRead,      RWMode::kWrite,
                                            RWMode::kRandRead,  RWMode::kRandWrite,
                                            RWMode::kReadWrite, RWMode::kRandRW};

  for (const RWMode mode : kAllModes) {
    EXPECT_EQ(JobConfig::ParseRWMode(JobConfig::ToString(mode)), mode);
  }
}

// --- EffectiveIODepth ---

TEST(JobConfigEffectiveDepth, SyncForcesOne) {
  JobConfig cfg;
  cfg.engine = "sync";
  cfg.iodepth = 32;
  EXPECT_EQ(cfg.EffectiveIODepth(), 1);
}

TEST(JobConfigEffectiveDepth, PsyncForcesOne) {
  JobConfig cfg;
  cfg.engine = "psync";
  cfg.iodepth = 16;
  EXPECT_EQ(cfg.EffectiveIODepth(), 1);
}

TEST(JobConfigEffectiveDepth, LibaioUsesConfigured) {
  JobConfig cfg;
  cfg.engine = "libaio";
  cfg.iodepth = 32;
  EXPECT_EQ(cfg.EffectiveIODepth(), 32);
}

TEST(JobConfigEffectiveDepth, IoUringUsesConfigured) {
  JobConfig cfg;
  cfg.engine = "io_uring";
  cfg.iodepth = 64;
  EXPECT_EQ(cfg.EffectiveIODepth(), 64);
}

TEST(JobConfigEffectiveDepth, SyncWithDepthOne) {
  JobConfig cfg;
  cfg.engine = "sync";
  cfg.iodepth = 1;
  EXPECT_EQ(cfg.EffectiveIODepth(), 1);
}

}  // namespace
}  // namespace cfio
