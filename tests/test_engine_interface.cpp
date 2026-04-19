/// @file test_engine_interface.cpp
/// @brief Unit tests for IO engine interface, factory, and all four engines.

#include "engine/engine_factory.h"
#include "engine/io_uring_engine.h"
#include "engine/libaio_engine.h"
#include "engine/psync_engine.h"
#include "engine/sync_engine.h"
#include "logging/logger.h"
#include "telemetry/aligned_buffer.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

namespace cfio {

// -- Logger environment: GTest manages init/shutdown around all tests --------

class LoggerEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    auto log_path =
        std::filesystem::path{::testing::TempDir()} / "cfio_engine_test.log";
    Logger::init(log_path, /*verbose=*/false);
  }
  void TearDown() override { Logger::shutdown(); }
};

namespace {

// -- Constants ---------------------------------------------------------------

constexpr size_t kBlockSize = 4096;
constexpr size_t kFileSize = 65536;
constexpr size_t kBlockCount = kFileSize / kBlockSize;
static_assert(kFileSize % kBlockSize == 0, "kFileSize must be a multiple of kBlockSize");
constexpr int kAsyncDepth = 4;

// -- Helpers -----------------------------------------------------------------

JobConfig MakeTestConfig(const std::string& engine_name,
                         const std::filesystem::path& filepath) {
  JobConfig cfg;
  cfg.name = "test";
  cfg.engine = engine_name;
  cfg.filename = filepath;
  cfg.block_size = kBlockSize;
  cfg.file_size = kFileSize;
  cfg.iodepth = 1;
  cfg.direct = false;
  cfg.alignment = kBlockSize;
  return cfg;
}

// Build an IORequest with a timestamp.
IORequest MakeRequest(uint64_t id, off_t offset, void* buffer, size_t length,
                      IODirection direction) {
  IORequest req{};
  req.id = id;
  req.offset = offset;
  req.buffer = buffer;
  req.length = length;
  req.direction = direction;
  req.submit_time = std::chrono::steady_clock::now();
  return req;
}

void ExpectSuccessfulCompletion(const IOCompletion& c, uint64_t expected_id,
                                IODirection expected_dir,
                                ssize_t expected_bytes) {
  EXPECT_EQ(c.id, expected_id);
  EXPECT_EQ(c.direction, expected_dir);
  EXPECT_EQ(c.bytes_transferred, expected_bytes);
  EXPECT_TRUE(c.success);
  EXPECT_EQ(c.error_code, 0);
}

bool IsKernelUnavailableError(int err) {
  return err == ENOSYS || err == EPERM;
}

std::string TryOpen(IEngineIO& engine, const JobConfig& config) {
  try {
    engine.Open(config);
    return {};
  } catch (const std::system_error& e) {
    if (IsKernelUnavailableError(e.code().value())) {
      return std::string{"kernel support unavailable: "} + e.what();
    }
    throw;
  }
}

class TempFile {
 public:
  TempFile() : TempFile(::testing::TempDir()) {}
  explicit TempFile(const std::string& dir) {
    std::string tmpl = dir + "/cfio_test_XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    int fd = ::mkstemp(buf.data());
    if (fd < 0) {
      throw std::system_error(errno, std::system_category(), "mkstemp");
    }
    path_ = std::string{buf.data()};

    std::vector<uint8_t> block(kBlockSize);
    for (size_t i = 0; i < kBlockCount; ++i) {
      std::memset(block.data(), static_cast<int>(i & 0xFF), kBlockSize);
      auto written =
          ::write(fd, block.data(), kBlockSize);
      if (written != static_cast<ssize_t>(kBlockSize)) {
        ::close(fd);
        throw std::runtime_error("short write in TempFile constructor");
      }
    }
    ::fsync(fd);
    ::close(fd);
  }

  ~TempFile() {
    if (!path_.empty()) {
      ::unlink(path_.c_str());
    }
  }

  TempFile(const TempFile&) = delete;
  TempFile& operator=(const TempFile&) = delete;

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

// ============================================================================
// Step 1 -- EngineFactory tests
// ============================================================================

TEST(EngineFactoryTest, CreatesPsync) {
  auto engine = EngineFactory::Create("psync");
  ASSERT_NE(engine, nullptr);
  EXPECT_NE(dynamic_cast<PsyncEngine*>(engine.get()), nullptr);
}

TEST(EngineFactoryTest, CreatesSync) {
  auto engine = EngineFactory::Create("sync");
  ASSERT_NE(engine, nullptr);
  EXPECT_NE(dynamic_cast<SyncEngine*>(engine.get()), nullptr);
}

TEST(EngineFactoryTest, CreatesIoUring) {
  auto engine = EngineFactory::Create("io_uring");
  ASSERT_NE(engine, nullptr);
  EXPECT_NE(dynamic_cast<IoUringEngine*>(engine.get()), nullptr);
}

TEST(EngineFactoryTest, CreatesLibaio) {
  auto engine = EngineFactory::Create("libaio");
  ASSERT_NE(engine, nullptr);
  EXPECT_NE(dynamic_cast<LibaioEngine*>(engine.get()), nullptr);
}

TEST(EngineFactoryTest, ThrowsOnUnknown) {
  EXPECT_THROW(EngineFactory::Create("bogus"), std::runtime_error);
}

TEST(EngineFactoryTest, KnownEnginesContainsAll) {
  const auto& engines = EngineFactory::KnownEngines();
  EXPECT_NE(std::find(engines.begin(), engines.end(), "psync"), engines.end());
  EXPECT_NE(std::find(engines.begin(), engines.end(), "sync"), engines.end());
  EXPECT_NE(std::find(engines.begin(), engines.end(), "io_uring"),
            engines.end());
  EXPECT_NE(std::find(engines.begin(), engines.end(), "libaio"), engines.end());
}

TEST(EngineFactoryTest, IsSynchronousEngine) {
  EXPECT_TRUE(EngineFactory::IsSynchronousEngine("psync"));
  EXPECT_TRUE(EngineFactory::IsSynchronousEngine("sync"));
  EXPECT_FALSE(EngineFactory::IsSynchronousEngine("io_uring"));
  EXPECT_FALSE(EngineFactory::IsSynchronousEngine("libaio"));
}

TEST(EngineFactoryTest, IsKnownEngine) {
  EXPECT_TRUE(EngineFactory::IsKnownEngine("psync"));
  EXPECT_TRUE(EngineFactory::IsKnownEngine("sync"));
  EXPECT_TRUE(EngineFactory::IsKnownEngine("io_uring"));
  EXPECT_TRUE(EngineFactory::IsKnownEngine("libaio"));
  EXPECT_FALSE(EngineFactory::IsKnownEngine("bogus"));
  EXPECT_FALSE(EngineFactory::IsKnownEngine(""));
}

// ============================================================================
// Step 2 -- Typed sync engine tests for PsyncEngine + SyncEngine
// ============================================================================

template <typename T>
class SyncEngineTypedTest : public ::testing::Test {
 protected:
  void SetUp() override { tmp_ = std::make_unique<TempFile>(); }
  void TearDown() override {
    engine_.Close();
    tmp_.reset();
  }

  JobConfig MakeConfig() {
    std::string name;
    if constexpr (std::is_same_v<T, PsyncEngine>) {
      name = "psync";
    } else {
      static_assert(std::is_same_v<T, SyncEngine>, "unhandled sync engine");
      name = "sync";
    }
    return MakeTestConfig(name, tmp_->path());
  }

  T engine_;
  std::unique_ptr<TempFile> tmp_;
};

using SyncEngineTypes = ::testing::Types<PsyncEngine, SyncEngine>;
TYPED_TEST_SUITE(SyncEngineTypedTest, SyncEngineTypes);

TYPED_TEST(SyncEngineTypedTest, WriteAndReadBack) {
  this->engine_.Open(this->MakeConfig());

  AlignedBuffer write_buf(kBlockSize, kBlockSize);
  AlignedBuffer read_buf(kBlockSize, kBlockSize);
  std::memset(write_buf.data(), 0xA5, kBlockSize);
  std::memset(read_buf.data(), 0x00, kBlockSize);

  // Write
  auto wreq =
      MakeRequest(1, 0, write_buf.data(), kBlockSize, IODirection::kWrite);
  this->engine_.SubmitIO(wreq);

  std::vector<IOCompletion> out;
  this->engine_.PollCompletions(1, 1, out);
  ASSERT_EQ(out.size(), 1u);
  ExpectSuccessfulCompletion(out[0], 1, IODirection::kWrite,
                             static_cast<ssize_t>(kBlockSize));

  // Read back into a separate buffer
  out.clear();
  auto rreq =
      MakeRequest(2, 0, read_buf.data(), kBlockSize, IODirection::kRead);
  this->engine_.SubmitIO(rreq);
  this->engine_.PollCompletions(1, 1, out);
  ASSERT_EQ(out.size(), 1u);
  ExpectSuccessfulCompletion(out[0], 2, IODirection::kRead,
                             static_cast<ssize_t>(kBlockSize));

  EXPECT_EQ(std::memcmp(read_buf.data(), write_buf.data(), kBlockSize), 0);
}

TYPED_TEST(SyncEngineTypedTest, CompletionFieldsOnWrite) {
  this->engine_.Open(this->MakeConfig());

  AlignedBuffer buf(kBlockSize, kBlockSize);
  std::memset(buf.data(), 0xBB, kBlockSize);

  auto req = MakeRequest(42, 0, buf.data(), kBlockSize, IODirection::kWrite);
  this->engine_.SubmitIO(req);

  std::vector<IOCompletion> out;
  this->engine_.PollCompletions(1, 1, out);
  ASSERT_EQ(out.size(), 1u);
  ExpectSuccessfulCompletion(out[0], 42, IODirection::kWrite,
                             static_cast<ssize_t>(kBlockSize));
  EXPECT_EQ(out[0].submit_time, req.submit_time);
}

TYPED_TEST(SyncEngineTypedTest, CompletionFieldsOnRead) {
  this->engine_.Open(this->MakeConfig());

  AlignedBuffer buf(kBlockSize, kBlockSize);
  std::memset(buf.data(), 0x00, kBlockSize);

  auto req = MakeRequest(99, 0, buf.data(), kBlockSize, IODirection::kRead);
  this->engine_.SubmitIO(req);

  std::vector<IOCompletion> out;
  this->engine_.PollCompletions(1, 1, out);
  ASSERT_EQ(out.size(), 1u);
  ExpectSuccessfulCompletion(out[0], 99, IODirection::kRead,
                             static_cast<ssize_t>(kBlockSize));
  EXPECT_EQ(out[0].submit_time, req.submit_time);
}

TYPED_TEST(SyncEngineTypedTest, OpenCloseLifecycle) {
  auto config = this->MakeConfig();

  this->engine_.Open(config);
  this->engine_.Close();

  this->engine_.Open(config);

  AlignedBuffer buf(kBlockSize, kBlockSize);
  std::memset(buf.data(), 0x00, kBlockSize);

  auto req = MakeRequest(1, 0, buf.data(), kBlockSize, IODirection::kRead);
  this->engine_.SubmitIO(req);

  std::vector<IOCompletion> out;
  this->engine_.PollCompletions(1, 1, out);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_TRUE(out[0].success);
}

TYPED_TEST(SyncEngineTypedTest, DoubleOpenThrows) {
  this->engine_.Open(this->MakeConfig());
  EXPECT_THROW(this->engine_.Open(this->MakeConfig()), std::runtime_error);
}

TYPED_TEST(SyncEngineTypedTest, SubmitBeforeOpenThrows) {
  AlignedBuffer buf(kBlockSize, kBlockSize);
  auto req = MakeRequest(1, 0, buf.data(), kBlockSize, IODirection::kRead);
  EXPECT_THROW(this->engine_.SubmitIO(req), std::runtime_error);
}

TYPED_TEST(SyncEngineTypedTest, DoubleSubmitWithoutPollThrows) {
  this->engine_.Open(this->MakeConfig());

  AlignedBuffer buf(kBlockSize, kBlockSize);
  auto req1 = MakeRequest(1, 0, buf.data(), kBlockSize, IODirection::kRead);
  this->engine_.SubmitIO(req1);

  auto req2 = MakeRequest(2, 0, buf.data(), kBlockSize, IODirection::kRead);
  EXPECT_THROW(this->engine_.SubmitIO(req2), std::logic_error);
}

TYPED_TEST(SyncEngineTypedTest, CloseBeforeOpenSafe) {
  EXPECT_NO_THROW(this->engine_.Close());
}

TYPED_TEST(SyncEngineTypedTest, DoubleCloseSafe) {
  this->engine_.Open(this->MakeConfig());
  this->engine_.Close();
  EXPECT_NO_THROW(this->engine_.Close());
}

// ============================================================================
// Step 3 -- Async engine smoke tests
// ============================================================================

template <typename T>
class AsyncEngineTypedTest : public ::testing::Test {
 protected:
  void SetUp() override { tmp_ = std::make_unique<TempFile>(); }
  void TearDown() override {
    engine_.Close();
    tmp_.reset();
  }

  JobConfig MakeConfig(int iodepth = kAsyncDepth) {
    std::string name;
    if constexpr (std::is_same_v<T, IoUringEngine>) {
      name = "io_uring";
    } else {
      static_assert(std::is_same_v<T, LibaioEngine>,
                    "unhandled async engine");
      name = "libaio";
    }
    auto cfg = MakeTestConfig(name, tmp_->path());
    cfg.iodepth = iodepth;
    return cfg;
  }

  T engine_;
  std::unique_ptr<TempFile> tmp_;
};

using AsyncEngineTypes = ::testing::Types<IoUringEngine, LibaioEngine>;
TYPED_TEST_SUITE(AsyncEngineTypedTest, AsyncEngineTypes);

TYPED_TEST(AsyncEngineTypedTest, AsyncWriteAndRead) {
  auto config = this->MakeConfig();
  if (auto skip = TryOpen(this->engine_, config); !skip.empty()) {
    GTEST_SKIP() << skip;
  }

  std::vector<AlignedBuffer> wbufs;
  for (int i = 0; i < kAsyncDepth; ++i) {
    wbufs.emplace_back(kBlockSize, kBlockSize);
    std::memset(wbufs.back().data(), static_cast<int>(0xA0 + i), kBlockSize);
  }

  for (int i = 0; i < kAsyncDepth; ++i) {
    auto offset = static_cast<off_t>(i * kBlockSize);
    auto req = MakeRequest(static_cast<uint64_t>(i), offset,
                           wbufs[static_cast<size_t>(i)].data(), kBlockSize,
                           IODirection::kWrite);
    this->engine_.SubmitIO(req);
  }

  std::vector<IOCompletion> out;
  this->engine_.PollCompletions(kAsyncDepth, kAsyncDepth, out);
  ASSERT_EQ(out.size(), static_cast<size_t>(kAsyncDepth));
  for (const auto& c : out) {
    EXPECT_TRUE(c.success) << "write completion id=" << c.id << " failed";
    EXPECT_EQ(c.bytes_transferred, static_cast<ssize_t>(kBlockSize));
    EXPECT_EQ(c.direction, IODirection::kWrite);
    EXPECT_EQ(c.error_code, 0);
  }

  std::vector<AlignedBuffer> rbufs;
  for (int i = 0; i < kAsyncDepth; ++i) {
    rbufs.emplace_back(kBlockSize, kBlockSize);
    std::memset(rbufs.back().data(), 0x00, kBlockSize);
  }

  for (int i = 0; i < kAsyncDepth; ++i) {
    auto offset = static_cast<off_t>(i * kBlockSize);
    auto req = MakeRequest(static_cast<uint64_t>(10 + i), offset,
                           rbufs[static_cast<size_t>(i)].data(), kBlockSize,
                           IODirection::kRead);
    this->engine_.SubmitIO(req);
  }

  out.clear();
  this->engine_.PollCompletions(kAsyncDepth, kAsyncDepth, out);
  ASSERT_EQ(out.size(), static_cast<size_t>(kAsyncDepth));
  for (const auto& c : out) {
    EXPECT_TRUE(c.success) << "read completion id=" << c.id << " failed";
    EXPECT_EQ(c.bytes_transferred, static_cast<ssize_t>(kBlockSize));
    EXPECT_EQ(c.direction, IODirection::kRead);
    EXPECT_EQ(c.error_code, 0);
  }

  for (const auto& c : out) {
    auto idx = static_cast<size_t>(c.id - 10);
    ASSERT_LT(idx, static_cast<size_t>(kAsyncDepth));
    EXPECT_EQ(
        std::memcmp(rbufs[idx].data(), wbufs[idx].data(), kBlockSize), 0)
        << "data mismatch at offset " << idx * kBlockSize;
  }
}

TYPED_TEST(AsyncEngineTypedTest, CompletionDirection) {
  auto config = this->MakeConfig();
  if (auto skip = TryOpen(this->engine_, config); !skip.empty()) {
    GTEST_SKIP() << skip;
  }

  AlignedBuffer wbuf(kBlockSize, kBlockSize);
  std::memset(wbuf.data(), 0xCC, kBlockSize);

  auto wreq = MakeRequest(1, 0, wbuf.data(), kBlockSize, IODirection::kWrite);
  this->engine_.SubmitIO(wreq);

  std::vector<IOCompletion> out;
  this->engine_.PollCompletions(1, 1, out);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].direction, IODirection::kWrite);
  EXPECT_EQ(out[0].submit_time, wreq.submit_time);

  AlignedBuffer rbuf(kBlockSize, kBlockSize);
  auto rreq = MakeRequest(2, 0, rbuf.data(), kBlockSize, IODirection::kRead);
  this->engine_.SubmitIO(rreq);

  out.clear();
  this->engine_.PollCompletions(1, 1, out);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].direction, IODirection::kRead);
  EXPECT_EQ(out[0].submit_time, rreq.submit_time);
}

TYPED_TEST(AsyncEngineTypedTest, OpenCloseLifecycle) {
  auto config = this->MakeConfig();
  if (auto skip = TryOpen(this->engine_, config); !skip.empty()) {
    GTEST_SKIP() << skip;
  }

  AlignedBuffer buf(kBlockSize, kBlockSize);
  std::memset(buf.data(), 0x00, kBlockSize);
  auto req = MakeRequest(1, 0, buf.data(), kBlockSize, IODirection::kRead);
  this->engine_.SubmitIO(req);

  std::vector<IOCompletion> out;
  this->engine_.PollCompletions(1, 1, out);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_TRUE(out[0].success);

  this->engine_.Close();

  this->engine_.Open(config);
  out.clear();
  auto req2 = MakeRequest(2, 0, buf.data(), kBlockSize, IODirection::kRead);
  this->engine_.SubmitIO(req2);
  this->engine_.PollCompletions(1, 1, out);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_TRUE(out[0].success);
}

TYPED_TEST(AsyncEngineTypedTest, DoubleOpenThrows) {
  auto config = this->MakeConfig();
  if (auto skip = TryOpen(this->engine_, config); !skip.empty()) {
    GTEST_SKIP() << skip;
  }
  EXPECT_THROW(this->engine_.Open(config), std::runtime_error);
}

TYPED_TEST(AsyncEngineTypedTest, SubmitBeforeOpenThrows) {
  AlignedBuffer buf(kBlockSize, kBlockSize);
  auto req = MakeRequest(1, 0, buf.data(), kBlockSize, IODirection::kRead);
  EXPECT_THROW(this->engine_.SubmitIO(req), std::runtime_error);
}

TYPED_TEST(AsyncEngineTypedTest, CloseBeforeOpenSafe) {
  EXPECT_NO_THROW(this->engine_.Close());
}

TYPED_TEST(AsyncEngineTypedTest, DoubleCloseSafe) {
  auto config = this->MakeConfig();
  if (auto skip = TryOpen(this->engine_, config); !skip.empty()) {
    GTEST_SKIP() << skip;
  }
  this->engine_.Close();
  EXPECT_NO_THROW(this->engine_.Close());
}


}  // namespace
}  // namespace cfio

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::AddGlobalTestEnvironment(new cfio::LoggerEnvironment);
  return RUN_ALL_TESTS();
}
