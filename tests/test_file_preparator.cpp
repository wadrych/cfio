/// @file test_file_preparator.cpp
/// @brief Unit tests for FilePreparator

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "config/job_config.h"
#include "orchestrator/file_preparator.h"

namespace cfio {
namespace {

constexpr size_t kBlockSize = 4096;
constexpr size_t kSmallSize = size_t{64} * 1024;

std::vector<char> ReadAll(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

class FilePreparatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    temp_dir_ = std::filesystem::path(::testing::TempDir()) / ("cfio_" + std::string(info->name()));
    std::filesystem::create_directories(temp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(temp_dir_); }

  JobConfig MakeConfig(const std::string& name, size_t file_size) {
    JobConfig cfg;
    cfg.name = name;
    cfg.engine = "psync";
    cfg.filename = temp_dir_ / (name + ".dat");
    cfg.block_size = kBlockSize;
    cfg.file_size = file_size;
    cfg.alignment = kBlockSize;
    return cfg;
  }

  std::filesystem::path temp_dir_;
};

TEST_F(FilePreparatorTest, CreateAndFillProducesRequestedSize) {
  const JobConfig cfg = MakeConfig("sized", kSmallSize);
  FilePreparator prep(/*keep_files=*/false);
  prep.CreateAndFill(cfg);

  ASSERT_TRUE(std::filesystem::exists(cfg.filename));
  EXPECT_EQ(std::filesystem::file_size(cfg.filename), kSmallSize);
}

TEST_F(FilePreparatorTest, CreatedFileHasNonZeroContent) {
  const JobConfig cfg = MakeConfig("content", kSmallSize);
  FilePreparator prep(/*keep_files=*/false);
  prep.CreateAndFill(cfg);

  const std::vector<char> data = ReadAll(cfg.filename);
  ASSERT_EQ(data.size(), kSmallSize);
  const bool any_nonzero = std::any_of(data.begin(), data.end(), [](char c) { return c != 0; });
  EXPECT_TRUE(any_nonzero);
}

TEST_F(FilePreparatorTest, CleanupDeletesCreatedFile) {
  const JobConfig cfg = MakeConfig("deleteme", kSmallSize);
  FilePreparator prep(/*keep_files=*/false);
  prep.CreateAndFill(cfg);
  ASSERT_TRUE(std::filesystem::exists(cfg.filename));

  prep.Cleanup();
  EXPECT_FALSE(std::filesystem::exists(cfg.filename));
}

TEST_F(FilePreparatorTest, KeepFilesPreservesFile) {
  const JobConfig cfg = MakeConfig("keep", kSmallSize);
  FilePreparator prep(/*keep_files=*/true);
  prep.CreateAndFill(cfg);

  prep.Cleanup();
  EXPECT_TRUE(std::filesystem::exists(cfg.filename));
}

TEST_F(FilePreparatorTest, CleanupPreservesPreExistingFile) {
  const JobConfig cfg = MakeConfig("preexisting", kSmallSize);
  {
    std::ofstream out(cfg.filename, std::ios::binary);
    out << "seed";
  }
  ASSERT_TRUE(std::filesystem::exists(cfg.filename));

  FilePreparator prep(/*keep_files=*/false);
  prep.CreateAndFill(cfg);
  prep.Cleanup();

  EXPECT_TRUE(std::filesystem::exists(cfg.filename));
}

TEST_F(FilePreparatorTest, DestructorBackstopCleansUp) {
  const JobConfig cfg = MakeConfig("backstop", kSmallSize);
  {
    FilePreparator prep(/*keep_files=*/false);
    prep.CreateAndFill(cfg);
    ASSERT_TRUE(std::filesystem::exists(cfg.filename));
  }
  EXPECT_FALSE(std::filesystem::exists(cfg.filename));
}

TEST_F(FilePreparatorTest, PartialFinalChunkExactSize) {
  const size_t odd_size = (1U << 20) + kBlockSize;
  const JobConfig cfg = MakeConfig("partial", odd_size);
  FilePreparator prep(/*keep_files=*/false);
  prep.CreateAndFill(cfg);

  EXPECT_EQ(std::filesystem::file_size(cfg.filename), odd_size);
}

TEST_F(FilePreparatorTest, CreateAndFillRejectsDirectoryPath) {
  JobConfig cfg = MakeConfig("dir", kSmallSize);
  cfg.filename = temp_dir_;
  FilePreparator prep(/*keep_files=*/false);

  EXPECT_THROW(prep.CreateAndFill(cfg), std::system_error);
}

}  // namespace
}  // namespace cfio
