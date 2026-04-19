/// @file psync_engine.cpp
/// @brief PsyncEngine implementation using pread/pwrite.

#include "engine/psync_engine.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <stdexcept>
#include <system_error>

#include "logging/logger.h"

namespace cfio {

PsyncEngine::~PsyncEngine() { Close(); }

void PsyncEngine::Open(const JobConfig& config) {
  if (fd_ >= 0) {
    throw std::runtime_error("PsyncEngine::Open -- engine is already open");
  }

  const std::string path_str = config.filename.string();
  const char* path = path_str.c_str();
  constexpr mode_t kFileMode = 0644;

  if (config.direct) {
    // Try with O_DIRECT first
    int flags = O_RDWR | O_CREAT | O_CLOEXEC | O_DIRECT;
    fd_ = ::open(path, flags, kFileMode);

    if (fd_ < 0) {
      int saved_errno = errno;
      if (saved_errno == EINVAL || saved_errno == EOPNOTSUPP) {
        Logger::get()->warn(
            "O_DIRECT not supported for '{}', falling back to buffered IO",
            path_str);
        flags = O_RDWR | O_CREAT | O_CLOEXEC;
        fd_ = ::open(path, flags, kFileMode);
        if (fd_ < 0) {
          throw std::system_error(errno, std::system_category(),
                                  "failed to open file '" + path_str + "'");
        }
        return;
      }
      throw std::system_error(saved_errno, std::system_category(),
                              "failed to open file '" + path_str +
                                  "' with O_DIRECT");
    }
    direct_effective_ = true;
  } else {
    int flags = O_RDWR | O_CREAT | O_CLOEXEC;
    fd_ = ::open(path, flags, kFileMode);
    if (fd_ < 0) {
      throw std::system_error(errno, std::system_category(),
                              "failed to open file '" + path_str + "'");
    }
  }
}

void PsyncEngine::SubmitIO(const IORequest& request) {
  if (fd_ < 0) {
    throw std::runtime_error("PsyncEngine::SubmitIO -- engine is not open");
  }
  if (has_completion_) {
    throw std::logic_error(
        "PsyncEngine::SubmitIO -- previous completion not yet polled");
  }

  // Retry lambda handles EINTR from signal delivery during the syscall
  auto retry_syscall = [](auto fn) {
    ssize_t r;
    do {
      r = fn();
    } while (r == -1 && errno == EINTR);
    return r;
  };

  ssize_t result = 0;
  if (request.direction == IODirection::kRead) {
    result = retry_syscall([&]() {
      return ::pread(fd_, request.buffer, request.length, request.offset);
    });
  } else {
    result = retry_syscall([&]() {
      return ::pwrite(fd_, request.buffer, request.length, request.offset);
    });
  }

  int saved_errno = errno;

  last_completion_.id = request.id;
  last_completion_.bytes_transferred = result >= 0 ? result : 0;
  last_completion_.direction = request.direction;
  last_completion_.submit_time = request.submit_time;

  if (result == -1) {
    last_completion_.success = false;
    last_completion_.error_code = saved_errno;
  } else if (result != static_cast<ssize_t>(request.length)) {
    // Short read/write -- not a syscall error but IO did not complete fully
    last_completion_.success = false;
    last_completion_.error_code = EIO;
  } else {
    last_completion_.success = true;
    last_completion_.error_code = 0;
  }

  has_completion_ = true;
}

void PsyncEngine::PollCompletions(int /*min_events*/, int max_events,
                                  std::vector<IOCompletion>& out) {
  if (!has_completion_ || max_events < 1) {
    return;
  }
  out.push_back(last_completion_);
  has_completion_ = false;
}

void PsyncEngine::Close() {
  if (fd_ >= 0) {
    int result = ::close(fd_);
    fd_ = -1;
    if (result == -1) {
      Logger::get()->warn("PsyncEngine::Close -- close failed with errno {}",
                          errno);
    }
  }
  has_completion_ = false;
}

bool PsyncEngine::IsDirectEnabled() const noexcept {
  return direct_effective_;
}

}  // namespace cfio
