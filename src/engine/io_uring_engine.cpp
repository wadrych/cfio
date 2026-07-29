/// @file io_uring_engine.cpp
/// @brief IoUringEngine implementation using io_uring SQE/CQE ring buffers.

#include "engine/io_uring_engine.h"

#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstring>
#include <stdexcept>
#include <system_error>

#include "engine/engine_utils.h"
#include "logging/logger.h"

namespace cfio {

IoUringEngine::~IoUringEngine() {
  Close();
}

void IoUringEngine::Open(const JobConfig& config) {
  if (fd_ >= 0 || ring_initialized_) {
    throw std::runtime_error("IoUringEngine::Open -- engine is already open");
  }

  if (config.iodepth < 1) {
    throw std::runtime_error("IoUringEngine::Open -- iodepth must be >= 1, got " +
                             std::to_string(config.iodepth));
  }

  auto open_result = OpenFileWithDirectFallback(config.filename, config.direct);
  fd_ = open_result.fd;
  direct_effective_ = open_result.direct_effective;

  const int ret = io_uring_queue_init(static_cast<unsigned>(config.iodepth), &ring_, 0);
  if (ret < 0) {
    if (::close(fd_) != 0) {
      Logger::Get()->warn("IoUringEngine::Open -- cleanup close failed: {}", ErrnoToString(errno));
    }
    fd_ = -1;
    direct_effective_ = false;
    throw std::system_error(-ret, std::system_category(), "io_uring_queue_init failed");
  }
  ring_initialized_ = true;

  iodepth_ = config.iodepth;
  in_flight_.reserve(static_cast<size_t>(iodepth_));
  cqe_batch_.resize(static_cast<size_t>(iodepth_));

  Logger::Get()->debug("io_uring ring initialized: requested={}, sq_entries={}, cq_entries={}",
                       config.iodepth, ring_.sq.ring_entries, ring_.cq.ring_entries);
}

void IoUringEngine::SubmitIO(const IORequest& request) {
  if (fd_ < 0 || !ring_initialized_) {
    throw std::runtime_error("IoUringEngine::SubmitIO -- engine is not open");
  }

  if (request.length > static_cast<size_t>(UINT_MAX)) {
    throw std::runtime_error(
        "IoUringEngine::SubmitIO -- request.length exceeds unsigned int range");
  }

  if (in_flight_.contains(request.id)) {
    throw std::logic_error("IoUringEngine::SubmitIO -- duplicate request id " +
                           std::to_string(request.id));
  }

  struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
  if (sqe == nullptr) {
    throw std::logic_error("IoUringEngine::SubmitIO -- SQ ring full, iodepth contract violated");
  }

  auto nbytes = static_cast<unsigned>(request.length);

  if (request.direction == IODirection::kRead) {
    io_uring_prep_read(sqe, fd_, request.buffer, nbytes, request.offset);
  } else {
    io_uring_prep_write(sqe, fd_, request.buffer, nbytes, request.offset);
  }

  io_uring_sqe_set_data64(sqe, request.id);

  in_flight_.emplace(request.id,
                     SubmitInfo{request.submit_time, request.direction, request.length});

  const int ret = io_uring_submit(&ring_);
  if (ret < 1) {
    if (ret < 0) {
      in_flight_.erase(request.id);
      throw std::system_error(-ret, std::system_category(), "io_uring_submit failed");
    }
    Logger::Get()->warn("io_uring_submit returned 0 -- SQE buffered");
  }
}

void IoUringEngine::PollCompletions(int min_events, int max_events,
                                    std::vector<IOCompletion>& out) {
  if (!ring_initialized_ || max_events < 1) {
    return;
  }

  if (min_events < 0) {
    min_events = 0;
  }

  out.reserve(out.size() + static_cast<size_t>(max_events));

  if (min_events > 0) {
    struct io_uring_cqe* cqe = nullptr;
    int ret = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
    do {
      ret = io_uring_wait_cqe_nr(&ring_, &cqe, static_cast<unsigned>(min_events));
    } while (ret == -EINTR || ret == -EAGAIN);

    if (ret < 0) {
      throw std::system_error(-ret, std::system_category(), "io_uring_wait_cqe_nr failed");
    }
  }

  auto batch_limit = static_cast<unsigned>(max_events);
  if (batch_limit > cqe_batch_.size()) {
    batch_limit = static_cast<unsigned>(cqe_batch_.size());
  }
  const unsigned nr = io_uring_peek_batch_cqe(&ring_, cqe_batch_.data(), batch_limit);

  for (unsigned i = 0; i < nr; ++i) {
    struct io_uring_cqe* c = cqe_batch_[i];
    uint64_t req_id = io_uring_cqe_get_data64(c);

    auto it = in_flight_.find(req_id);
    if (it == in_flight_.end()) {
      Logger::Get()->error("IoUringEngine::PollCompletions -- unexpected CQE for id {}", req_id);
      continue;
    }

    const SubmitInfo& info = it->second;
    IOCompletion completion{};
    completion.id = req_id;
    completion.direction = info.direction;
    completion.submit_time = info.submit_time;

    if (c->res < 0) {
      completion.bytes_transferred = 0;
      completion.success = false;
      completion.error_code = -c->res;
    } else {
      completion.bytes_transferred = c->res;
      completion.success = static_cast<size_t>(c->res) == info.length;
      completion.error_code = completion.success ? 0 : EIO;
    }

    in_flight_.erase(it);
    out.push_back(completion);
  }

  io_uring_cq_advance(&ring_, nr);
}

void IoUringEngine::Close() {
  if (ring_initialized_) {
    if (!in_flight_.empty()) {
      Logger::Get()->warn("IoUringEngine::Close -- draining {} pending in-flight IOs",
                          in_flight_.size());

      struct io_uring_cqe* cqe = nullptr;
      while (!in_flight_.empty()) {
        int ret = 0;
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
        do {
          ret = io_uring_wait_cqe(&ring_, &cqe);
        } while (ret == -EINTR);

        if (ret < 0) {
          Logger::Get()->error("IoUringEngine::Close -- drain wait failed with {}", -ret);
          break;
        }

        const uint64_t req_id = io_uring_cqe_get_data64(cqe);
        in_flight_.erase(req_id);
        io_uring_cqe_seen(&ring_, cqe);
      }
    }

    io_uring_queue_exit(&ring_);
    ring_initialized_ = false;
  }

  if (fd_ >= 0) {
    const int result = ::close(fd_);
    fd_ = -1;
    if (result == -1) {
      Logger::Get()->warn("IoUringEngine::Close -- close failed with errno {}", errno);
    }
  }

  direct_effective_ = false;
  iodepth_ = 0;
  in_flight_.clear();
  cqe_batch_.clear();
}

bool IoUringEngine::IsDirectEnabled() const noexcept {
  return direct_effective_;
}

}  // namespace cfio
