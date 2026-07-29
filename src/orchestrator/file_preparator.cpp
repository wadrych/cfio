/// @file file_preparator.cpp
/// @brief Implementation of FilePreparator

#include "orchestrator/file_preparator.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>
#include <vector>

#include "engine/engine_utils.h"
#include "logging/logger.h"

namespace cfio {
namespace {

// random data written in 1MB chunks
constexpr size_t kChunkSize = 1U << 20;

void WriteFully(int fd, const char* buf, size_t len) {
  size_t written = 0;
  while (written < len) {
    const ssize_t n = RetryOnEintr([&]() { return ::write(fd, buf + written, len - written); });
    if (n < 0) {
      throw std::system_error(errno, std::system_category(), "write during file fill failed");
    }
    written += static_cast<size_t>(n);
  }
}

void ReserveSpace(int fd, size_t file_size, const std::string& path_str) {
  if (::fallocate(fd, 0, 0, static_cast<off_t>(file_size)) == 0) {
    return;
  }
  const int saved_errno = errno;
  if (saved_errno == EOPNOTSUPP || saved_errno == ENOSYS) {
    Logger::Get()->warn("fallocate unsupported for '{}', space reserved by writes", path_str);
    return;
  }
  throw std::system_error(saved_errno, std::system_category(),
                          "fallocate failed for '" + path_str + "'");
}

void FillRandom(int fd, size_t file_size) {
  std::mt19937_64 rng(std::random_device{}());
  std::vector<std::uint64_t> chunk(kChunkSize / sizeof(std::uint64_t));

  size_t remaining = file_size;
  while (remaining > 0) {
    const size_t to_write = std::min(kChunkSize, remaining);
    for (std::uint64_t& word : chunk) {
      word = rng();
    }
    WriteFully(fd, reinterpret_cast<const char*>(chunk.data()), to_write);
    remaining -= to_write;
  }
}

}  // namespace

FilePreparator::FilePreparator(bool keep_files) : keep_files_(keep_files) {
}

FilePreparator::~FilePreparator() {
  if (cleaned_ || keep_files_) {
    return;
  }
  for (const CreatedFile& file : created_) {
    if (!file.pre_existed) {
      std::error_code ec;
      std::filesystem::remove(file.path, ec);
    }
  }
}

void FilePreparator::CreateAndFill(const JobConfig& config) {
  const std::string path_str = config.filename.string();

  std::error_code exists_ec;
  const bool pre_existed = std::filesystem::exists(config.filename, exists_ec);

  const int fd = ::open(path_str.c_str(), O_CREAT | O_WRONLY | O_CLOEXEC, 0644);
  if (fd < 0) {
    throw std::system_error(errno, std::system_category(),
                            "failed to create file '" + path_str + "'");
  }

  try {
    ReserveSpace(fd, config.file_size, path_str);
    FillRandom(fd, config.file_size);

    if (::fsync(fd) != 0) {
      throw std::system_error(errno, std::system_category(), "fsync failed for '" + path_str + "'");
    }
  } catch (...) {
    ::close(fd);
    throw;
  }

  if (::close(fd) != 0) {
    const int saved_errno = errno;
    Logger::Get()->warn("FilePreparator::CreateAndFill - close failed with errno {}", saved_errno);
  }

  created_.push_back({config.filename, pre_existed});
}

void FilePreparator::Cleanup() {
  if (cleaned_) {
    return;
  }
  cleaned_ = true;

  for (const CreatedFile& file : created_) {
    if (keep_files_) {
      Logger::Get()->info("keeping file '{}'", file.path.string());
      continue;
    }
    if (file.pre_existed) {
      continue;
    }
    std::error_code ec;
    std::filesystem::remove(file.path, ec);
    if (ec) {
      Logger::Get()->warn("failed to delete '{}': {}", file.path.string(), ec.message());
    }
  }
}

}  // namespace cfio
