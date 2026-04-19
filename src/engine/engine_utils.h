#ifndef CFIO_ENGINE_ENGINE_UTILS_H_
#define CFIO_ENGINE_ENGINE_UTILS_H_

/// @file engine_utils.h
/// @brief Shared utilities for IO engine implementations.

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <string>
#include <system_error>

#include "logging/logger.h"

namespace cfio {

/// @brief Result of opening a file with optional O_DIRECT.
struct OpenResult {
  int fd = -1;
  bool direct_effective = false;
};

/// @brief Open a file for IO with optional O_DIRECT and automatic fallback.
///
/// @param file_path         Path to the file.
/// @param direct_requested  Whether to attempt O_DIRECT.
/// @return OpenResult with the file descriptor and effective direct flag.
/// @throws std::system_error on open failure.
inline OpenResult OpenFileWithDirectFallback(const std::filesystem::path& file_path,
                                             bool direct_requested) {
  const std::string path_str = file_path.string();
  const char* path = path_str.c_str();
  constexpr mode_t kFileMode = 0644;

  if (direct_requested) {
    int flags = O_RDWR | O_CREAT | O_CLOEXEC | O_DIRECT;
    int fd = ::open(path, flags, kFileMode);

    if (fd < 0) {
      const int saved_errno = errno;
      if (saved_errno == EINVAL || saved_errno == EOPNOTSUPP) {
        Logger::get()->warn("O_DIRECT not supported for '{}', falling back to buffered IO",
                            path_str);
        flags = O_RDWR | O_CREAT | O_CLOEXEC;
        fd = ::open(path, flags, kFileMode);
        if (fd < 0) {
          throw std::system_error(errno, std::system_category(),
                                  "failed to open file '" + path_str + "'");
        }
        return {fd, false};
      }
      throw std::system_error(saved_errno, std::system_category(),
                              "failed to open file '" + path_str + "' with O_DIRECT");
    }
    return {fd, true};
  }

  const int flags = O_RDWR | O_CREAT | O_CLOEXEC;
  const int fd = ::open(path, flags, kFileMode);
  if (fd < 0) {
    throw std::system_error(errno, std::system_category(),
                            "failed to open file '" + path_str + "'");
  }
  return {fd, false};
}

/// @brief Retry a blocking syscall that can be interrupted by signals.
///
/// @tparam Fn  Callable returning ssize_t.
/// @param fn   The syscall wrapper to retry.
/// @return The syscall result, or -1 with errno set on non-EINTR failure.
template<typename Fn>
ssize_t RetryOnEintr(Fn fn) {
  ssize_t r = 0;
  do {
    r = fn();
  } while (r == -1 && errno == EINTR);
  return r;
}

}  // namespace cfio

#endif  // CFIO_ENGINE_ENGINE_UTILS_H_
