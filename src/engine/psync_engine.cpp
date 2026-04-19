/// @file psync_engine.cpp
/// @brief PsyncEngine implementation using pread/pwrite.

#include "engine/psync_engine.h"

#include <unistd.h>

#include <cerrno>
#include <stdexcept>

#include "engine/engine_utils.h"
#include "logging/logger.h"

namespace cfio {

PsyncEngine::~PsyncEngine() {
  Close();
}

void PsyncEngine::Open(const JobConfig& config) {
  if (fd_ >= 0) {
    throw std::runtime_error("PsyncEngine::Open -- engine is already open");
  }

  auto result = OpenFileWithDirectFallback(config.filename, config.direct);
  fd_ = result.fd;
  direct_effective_ = result.direct_effective;
}

void PsyncEngine::SubmitIO(const IORequest& request) {
  if (fd_ < 0) {
    throw std::runtime_error("PsyncEngine::SubmitIO -- engine is not open");
  }
  if (has_completion_) {
    throw std::logic_error("PsyncEngine::SubmitIO -- previous completion not yet polled");
  }

  ssize_t result = 0;
  switch (request.direction) {
    case IODirection::kRead:
      result = RetryOnEintr(
          [&]() { return ::pread(fd_, request.buffer, request.length, request.offset); });
      break;
    case IODirection::kWrite:
      result = RetryOnEintr(
          [&]() { return ::pwrite(fd_, request.buffer, request.length, request.offset); });
      break;
    default:
      throw std::logic_error("PsyncEngine::SubmitIO -- unknown IO direction");
  }

  const int saved_errno = errno;

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
    const int result = ::close(fd_);
    fd_ = -1;
    if (result == -1) {
      const int saved_errno = errno;
      Logger::get()->warn("PsyncEngine::Close -- close failed with errno {}", saved_errno);
    }
  }
  has_completion_ = false;
  direct_effective_ = false;
}

bool PsyncEngine::IsDirectEnabled() const noexcept {
  return direct_effective_;
}

}  // namespace cfio
