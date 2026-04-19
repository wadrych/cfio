/// @file sync_engine.cpp
/// @brief SyncEngine implementation using lseek + read/write.

#include "engine/sync_engine.h"

#include <unistd.h>

#include <cerrno>
#include <stdexcept>

#include "engine/engine_utils.h"
#include "logging/logger.h"

namespace cfio {

SyncEngine::~SyncEngine() {
  Close();
}

void SyncEngine::Open(const JobConfig& config) {
  if (fd_ >= 0) {
    throw std::runtime_error("SyncEngine::Open -- engine is already open");
  }

  auto result = OpenFileWithDirectFallback(config.filename, config.direct);
  fd_ = result.fd;
  direct_effective_ = result.direct_effective;
}

void SyncEngine::SubmitIO(const IORequest& request) {
  if (fd_ < 0) {
    throw std::runtime_error("SyncEngine::SubmitIO -- engine is not open");
  }
  if (has_completion_) {
    throw std::logic_error("SyncEngine::SubmitIO -- previous completion not yet polled");
  }

  const off_t seek_result = ::lseek(fd_, request.offset, SEEK_SET);
  if (seek_result == static_cast<off_t>(-1)) {
    const int saved_errno = errno;
    last_completion_.id = request.id;
    last_completion_.bytes_transferred = 0;
    last_completion_.direction = request.direction;
    last_completion_.submit_time = request.submit_time;
    last_completion_.success = false;
    last_completion_.error_code = saved_errno;
    has_completion_ = true;
    return;
  }

  ssize_t result = 0;
  switch (request.direction) {
    case IODirection::kRead:
      result = RetryOnEintr([&]() { return ::read(fd_, request.buffer, request.length); });
      break;
    case IODirection::kWrite:
      result = RetryOnEintr([&]() { return ::write(fd_, request.buffer, request.length); });
      break;
    default:
      throw std::logic_error("SyncEngine::SubmitIO -- unknown IO direction");
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
    last_completion_.success = false;
    last_completion_.error_code = EIO;
  } else {
    last_completion_.success = true;
    last_completion_.error_code = 0;
  }

  has_completion_ = true;
}

void SyncEngine::PollCompletions(int /*min_events*/, int max_events,
                                 std::vector<IOCompletion>& out) {
  if (!has_completion_ || max_events < 1) {
    return;
  }
  out.push_back(last_completion_);
  has_completion_ = false;
}

void SyncEngine::Close() {
  if (fd_ >= 0) {
    const int result = ::close(fd_);
    fd_ = -1;
    if (result == -1) {
      const int saved_errno = errno;
      Logger::Get()->warn("SyncEngine::Close -- close failed with errno {}", saved_errno);
    }
  }
  has_completion_ = false;
  direct_effective_ = false;
}

bool SyncEngine::IsDirectEnabled() const noexcept {
  return direct_effective_;
}

}  // namespace cfio
