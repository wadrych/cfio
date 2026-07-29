/// @file test_engine_utils.cpp
/// @brief Unit tests for shared engine utilities

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "engine/engine_utils.h"

namespace cfio {
namespace {

// -- RetryOnEintr ------------------------------------------------------------

TEST(RetryOnEintrTest, ReturnsValueOnFirstSuccess) {
  int calls = 0;
  const ssize_t result = RetryOnEintr([&calls]() -> ssize_t {
    ++calls;
    return 4096;
  });

  EXPECT_EQ(result, 4096);
  EXPECT_EQ(calls, 1);
}

TEST(RetryOnEintrTest, TreatsZeroAsSuccess) {
  int calls = 0;
  const ssize_t result = RetryOnEintr([&calls]() -> ssize_t {
    ++calls;
    return 0;
  });

  EXPECT_EQ(result, 0);
  EXPECT_EQ(calls, 1);
}

TEST(RetryOnEintrTest, RetriesWhileInterrupted) {
  int calls = 0;
  const ssize_t result = RetryOnEintr([&calls]() -> ssize_t {
    ++calls;
    if (calls < 3) {
      errno = EINTR;
      return -1;
    }
    return 512;
  });

  EXPECT_EQ(result, 512);
  EXPECT_EQ(calls, 3);
}

TEST(RetryOnEintrTest, DoesNotRetryOnOtherErrors) {
  int calls = 0;
  const ssize_t result = RetryOnEintr([&calls]() -> ssize_t {
    ++calls;
    errno = EIO;
    return -1;
  });

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EIO);
  EXPECT_EQ(calls, 1);
}

// -- OpenFileWithDirectFallback ----------------------------------------------

bool DirectIoRejectedByFs(const std::filesystem::path& path) {
  const int fd = ::open(path.c_str(), O_RDWR | O_DIRECT);
  if (fd >= 0) {
    ::close(fd);
    return false;
  }
  return errno == EINVAL || errno == EOPNOTSUPP;
}

class OpenFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    temp_dir_ = std::filesystem::path(::testing::TempDir()) / ("cfio_" + std::string(info->name()));
    std::filesystem::create_directories(temp_dir_);
  }

  void TearDown() override { std::filesystem::remove_all(temp_dir_); }

  [[nodiscard]] std::filesystem::path FilePath() const { return temp_dir_ / "target.dat"; }

  std::filesystem::path temp_dir_;
};

TEST_F(OpenFileTest, BufferedOpenCreatesFileAndReportsNoDirect) {
  const OpenResult result = OpenFileWithDirectFallback(FilePath(), /*direct_requested=*/false);

  ASSERT_GE(result.fd, 0);
  EXPECT_FALSE(result.direct_effective);
  EXPECT_TRUE(std::filesystem::exists(FilePath()));
  ::close(result.fd);
}

TEST_F(OpenFileTest, BufferedOpenThrowsWhenDirectoryMissing) {
  const std::filesystem::path bad = temp_dir_ / "no_such_dir" / "target.dat";
  EXPECT_THROW(OpenFileWithDirectFallback(bad, /*direct_requested=*/false), std::system_error);
}

TEST_F(OpenFileTest, DirectOpenThrowsWhenDirectoryMissing) {
  const std::filesystem::path bad = temp_dir_ / "no_such_dir" / "target.dat";
  EXPECT_THROW(OpenFileWithDirectFallback(bad, /*direct_requested=*/true), std::system_error);
}

TEST_F(OpenFileTest, DirectOpenSucceedsOnRegularFile) {
  const OpenResult result = OpenFileWithDirectFallback(FilePath(), /*direct_requested=*/true);

  ASSERT_GE(result.fd, 0);
  ::close(result.fd);
}

TEST(OpenFileDirectFallbackTest, FallsBackToBufferedOnTmpfs) {
  struct stat st {};
  if (::stat("/dev/shm", &st) != 0 || !S_ISDIR(st.st_mode)) {
    GTEST_SKIP() << "/dev/shm not available";
  }
  if (::access("/dev/shm", W_OK) != 0) {
    GTEST_SKIP() << "/dev/shm not writable";
  }

  const std::filesystem::path path = "/dev/shm/cfio_engine_utils_fallback.dat";
  {
    const int fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
    ASSERT_GE(fd, 0);
    ::close(fd);
  }

  if (!DirectIoRejectedByFs(path)) {
    std::filesystem::remove(path);
    GTEST_SKIP() << "kernel tmpfs accepts O_DIRECT, cannot exercise fallback path";
  }

  const OpenResult result = OpenFileWithDirectFallback(path, /*direct_requested=*/true);
  EXPECT_GE(result.fd, 0);
  EXPECT_FALSE(result.direct_effective);

  ::close(result.fd);
  std::filesystem::remove(path);
}

}  // namespace
}  // namespace cfio
