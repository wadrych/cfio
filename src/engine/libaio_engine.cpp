/// @file libaio_engine.cpp
/// @brief LibaioEngine implementation using Linux AIO iocb/io_getevents.

#include "engine/libaio_engine.h"

#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <system_error>

#include "engine/engine_utils.h"
#include "logging/logger.h"

static_assert(sizeof(uintptr_t) >= sizeof(uint64_t),
              "request id round-trip through void* requires 64-bit pointers");

namespace cfio {

LibaioEngine::~LibaioEngine() {
  Close();
}

void LibaioEngine::Open(const JobConfig& config) {
  if (fd_ >= 0 || ctx_ != nullptr) {
    throw std::runtime_error("LibaioEngine::Open -- engine is already open");
  }

  if (config.iodepth < 1) {
    throw std::runtime_error("LibaioEngine::Open -- iodepth must be >= 1, got " +
                             std::to_string(config.iodepth));
  }

  auto open_result = OpenFileWithDirectFallback(config.filename, config.direct);
  fd_ = open_result.fd;
  direct_effective_ = open_result.direct_effective;

  if (!direct_effective_) {
    Logger::Get()->warn(
        "LibaioEngine: O_DIRECT not active -- kernel may execute IOs "
        "synchronously despite async submission");
  }

  const int ret = io_setup(config.iodepth, &ctx_);
  if (ret < 0) {
    if (::close(fd_) != 0) {
      Logger::Get()->warn("LibaioEngine::Open -- cleanup close failed: {}", ErrnoToString(errno));
    }
    fd_ = -1;
    ctx_ = nullptr;
    direct_effective_ = false;
    if (ret == -EAGAIN) {
      throw std::runtime_error(
          "LibaioEngine::Open -- io_setup EAGAIN: iodepth may exceed "
          "/proc/sys/fs/aio-max-nr");
    }
    throw std::system_error(-ret, std::system_category(), "LibaioEngine::Open -- io_setup failed");
  }

  iodepth_ = config.iodepth;
  auto depth = static_cast<size_t>(iodepth_);

  iocb_pool_.resize(depth);
  free_iocbs_.clear();
  free_iocbs_.reserve(depth);
  for (size_t i = 0; i < depth; ++i) {
    free_iocbs_.push_back(&iocb_pool_[i]);
  }

  events_.resize(depth);
  in_flight_.reserve(depth);

  Logger::Get()->debug("LibaioEngine: AIO context initialized, iodepth={}", iodepth_);
}

void LibaioEngine::SubmitIO(const IORequest& request) {
  if (fd_ < 0 || ctx_ == nullptr) {
    throw std::runtime_error("LibaioEngine::SubmitIO -- engine is not open");
  }

  if (in_flight_.contains(request.id)) {
    throw std::logic_error("LibaioEngine::SubmitIO -- duplicate request id " +
                           std::to_string(request.id));
  }

  if (free_iocbs_.empty()) {
    throw std::logic_error("LibaioEngine::SubmitIO -- iocb pool empty, iodepth contract violated");
  }

  struct iocb* iocb_ptr = free_iocbs_.back();
  free_iocbs_.pop_back();

  if (request.direction == IODirection::kRead) {
    io_prep_pread(iocb_ptr, fd_, request.buffer, request.length, request.offset);
  } else {
    io_prep_pwrite(iocb_ptr, fd_, request.buffer, request.length, request.offset);
  }

  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  iocb_ptr->data = reinterpret_cast<void*>(static_cast<uintptr_t>(request.id));

  in_flight_.emplace(request.id,
                     SubmitInfo{request.submit_time, request.direction, request.length});

  std::array<struct iocb*, 1> iocbs = {iocb_ptr};
  const int ret = io_submit(ctx_, 1, iocbs.data());

  if (ret != 1) {
    in_flight_.erase(request.id);
    free_iocbs_.push_back(iocb_ptr);
    if (ret == -EAGAIN) {
      throw std::system_error(EAGAIN, std::system_category(),
                              "LibaioEngine::SubmitIO -- io_submit EAGAIN, AIO limit reached");
    }
    const int err = (ret < 0) ? -ret : EIO;
    throw std::system_error(
        err, std::system_category(),
        "LibaioEngine::SubmitIO -- io_submit failed, ret=" + std::to_string(ret));
  }
}

void LibaioEngine::PollCompletions(int min_events, int max_events, std::vector<IOCompletion>& out) {
  if (ctx_ == nullptr || max_events < 1) {
    return;
  }

  if (min_events < 0) {
    min_events = 0;
  }

  min_events = std::min(min_events, static_cast<int>(in_flight_.size()));
  max_events = std::min(max_events, static_cast<int>(events_.size()));

  if (min_events < 1 && in_flight_.empty()) {
    return;
  }

  out.reserve(out.size() + static_cast<size_t>(max_events));

  int nr = 0;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
  do {
    nr = io_getevents(ctx_, min_events, max_events, events_.data(), nullptr);
  } while (nr == -EINTR);

  if (nr < 0) {
    throw std::system_error(-nr, std::system_category(),
                            "LibaioEngine::PollCompletions -- io_getevents failed");
  }

  for (int i = 0; i < nr; ++i) {
    auto req_id =
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(events_[static_cast<size_t>(i)].data));

    auto it = in_flight_.find(req_id);
    if (it == in_flight_.end()) {
      Logger::Get()->error("LibaioEngine::PollCompletions -- unexpected event for id {}", req_id);
      free_iocbs_.push_back(events_[static_cast<size_t>(i)].obj);
      continue;
    }

    const SubmitInfo& info = it->second;
    IOCompletion completion{};
    completion.id = req_id;
    completion.direction = info.direction;
    completion.submit_time = info.submit_time;

    auto result = static_cast<int64_t>(events_[static_cast<size_t>(i)].res);

    if (result < 0) {
      completion.bytes_transferred = 0;
      completion.success = false;
      completion.error_code = static_cast<int>(-result);
    } else {
      completion.bytes_transferred = static_cast<ssize_t>(result);
      completion.success = static_cast<size_t>(result) == info.length;
      completion.error_code = completion.success ? 0 : EIO;
    }

    auto res2 = static_cast<int64_t>(events_[static_cast<size_t>(i)].res2);
    if (res2 != 0) {
      Logger::Get()->warn("LibaioEngine::PollCompletions -- res2={} for id {}", res2, req_id);
      completion.success = false;
      if (completion.error_code == 0) {
        completion.error_code = static_cast<int>(res2);
      }
    }

    free_iocbs_.push_back(events_[static_cast<size_t>(i)].obj);
    in_flight_.erase(it);
    out.push_back(completion);
  }
}

void LibaioEngine::Close() {
  if (ctx_ != nullptr && !in_flight_.empty()) {
    Logger::Get()->warn("LibaioEngine::Close -- draining {} pending in-flight IOs",
                        in_flight_.size());

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    while (!in_flight_.empty()) {
      auto remaining = deadline - std::chrono::steady_clock::now();
      if (remaining <= std::chrono::nanoseconds(0)) {
        break;
      }

      auto sec = std::chrono::duration_cast<std::chrono::seconds>(remaining);
      auto nsec = remaining - sec;
      struct timespec ts = {.tv_sec = static_cast<time_t>(sec.count()),
                            .tv_nsec = static_cast<decltype(timespec::tv_nsec)>(nsec.count())};

      const int nr = io_getevents(ctx_, 1, static_cast<int>(events_.size()), events_.data(), &ts);

      if (nr == -EINTR) {
        continue;
      }
      if (nr == 0) {
        break;
      }
      if (nr < 0) {
        Logger::Get()->error("LibaioEngine::Close -- io_getevents drain error {}", -nr);
        break;
      }

      for (int i = 0; i < nr; ++i) {
        auto req_id = static_cast<uint64_t>(
            reinterpret_cast<uintptr_t>(events_[static_cast<size_t>(i)].data));
        in_flight_.erase(req_id);
      }
    }

    if (!in_flight_.empty()) {
      Logger::Get()->error("LibaioEngine::Close -- {} IOs not drained, attempting io_cancel",
                           in_flight_.size());

      for (auto& cb : iocb_pool_) {
        auto id = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(cb.data));
        if (in_flight_.contains(id)) {
          struct io_event cancel_event {};
          const int rc = io_cancel(ctx_, &cb, &cancel_event);
          if (rc < 0 && rc != -EINVAL) {
            Logger::Get()->warn("LibaioEngine::Close -- io_cancel id={} returned {}", id, -rc);
          }
        }
      }
    }
  }

  if (ctx_ != nullptr) {
    const int ret = io_destroy(ctx_);
    if (ret < 0) {
      Logger::Get()->warn("LibaioEngine::Close -- io_destroy returned {}", -ret);
    }
    ctx_ = nullptr;
  }

  if (fd_ >= 0) {
    const int result = ::close(fd_);
    fd_ = -1;
    if (result == -1) {
      Logger::Get()->warn("LibaioEngine::Close -- close failed with errno {}", errno);
    }
  }

  direct_effective_ = false;
  iodepth_ = 0;
  iocb_pool_.clear();
  free_iocbs_.clear();
  events_.clear();
  in_flight_.clear();
}

bool LibaioEngine::IsDirectEnabled() const noexcept {
  return direct_effective_;
}

}  // namespace cfio
