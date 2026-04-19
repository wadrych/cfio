#ifndef CFIO_ENGINE_IO_URING_ENGINE_H_
#define CFIO_ENGINE_IO_URING_ENGINE_H_

/// @file io_uring_engine.h
/// @brief Asynchronous IO engine using Linux io_uring.

#include <liburing.h>

#include <unordered_map>
#include <vector>

#include "common/types.h"
#include "engine/i_engine_io.h"
#include "engine/submit_info.h"

namespace cfio {

/// @brief Asynchronous IO engine using io_uring SQE/CQE ring buffers.
class IoUringEngine : public IEngineIO {
 public:
  IoUringEngine() = default;
  ~IoUringEngine() override;

  IoUringEngine(const IoUringEngine&) = delete;
  IoUringEngine& operator=(const IoUringEngine&) = delete;
  IoUringEngine(IoUringEngine&&) = delete;
  IoUringEngine& operator=(IoUringEngine&&) = delete;

  void Open(const JobConfig& config) override;

  /// @brief Submit a single IO to the io_uring submission queue.
  ///
  /// @param request  The IO operation to submit.
  /// @throws std::logic_error if the SQ ring is full or request.id is duplicate.
  /// @throws std::system_error if io_uring_submit fails.
  /// @throws std::runtime_error if the engine is not open.
  void SubmitIO(const IORequest& request) override;

  void PollCompletions(int min_events, int max_events, std::vector<IOCompletion>& out) override;
  void Close() override;
  [[nodiscard]] bool IsDirectEnabled() const noexcept override;

 private:
  struct io_uring ring_ {};
  int fd_ = -1;
  bool direct_effective_ = false;
  bool ring_initialized_ = false;
  int iodepth_ = 0;
  std::unordered_map<uint64_t, SubmitInfo> in_flight_;
  std::vector<struct io_uring_cqe*> cqe_batch_;
};

}  // namespace cfio

#endif  // CFIO_ENGINE_IO_URING_ENGINE_H_
