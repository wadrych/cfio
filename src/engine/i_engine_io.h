#ifndef CFIO_ENGINE_I_ENGINE_IO_H_
#define CFIO_ENGINE_I_ENGINE_IO_H_

/// @file i_engine_io.h
/// @brief Abstract interface for all kernel-based IO engines.

#include <vector>

#include "common/types.h"
#include "config/job_config.h"

namespace cfio {

/// @brief Abstract interface for submitting IO requests and polling completions.
///
/// All kernel-based engines (sync, psync, libaio, io_uring) implement this
/// interface. The design uses a unified async model: even synchronous engines
/// use SubmitIO/PollCompletions so the WorkerThread has a single loop.
///
/// Lifecycle: Create → Open → (SubmitIO / PollCompletions loop) → Close.
class IEngineIO {
 public:
  virtual ~IEngineIO() = default;

  /// @brief Open a file and initialise engine-specific state.
  ///
  /// The engine reads config.filename for the file path, config.direct for
  /// O_DIRECT, config.alignment for buffer/offset alignment, and
  /// config.iodepth for async queue sizing.
  ///
  /// @param config  Job configuration containing file path, engine settings.
  /// @throws std::system_error on OS-level open failure.
  /// @throws std::runtime_error on config precondition violation.
  virtual void Open(const JobConfig& config) = 0;

  /// @brief Submit a single IO operation.
  ///
  /// For sync/psync engines, this executes the syscall immediately and stores
  /// the completion internally. The caller retrieves it via PollCompletions.
  ///
  /// For async engines, this enqueues the operation into
  /// the kernel submission queue. The engine uses its own internal file
  /// descriptor; the caller does not need to provide one.
  ///
  /// @param request  The IO operation to submit. The engine reads all fields
  ///                 but does not modify the request.
  /// @throws std::system_error on submission failure (e.g., queue full).
  virtual void SubmitIO(const IORequest& request) = 0;

  /// @brief Block until completions are available, then append them to @p out.
  ///
  /// Waits until at least @p min_events completions are ready, then appends
  /// up to @p max_events to the output vector. For sync/psync engines this
  /// appends the single buffered completion immediately.
  ///
  /// Each appended IOCompletion carries submit_time and direction from the
  /// original IORequest for latency calculation and error tracking.
  ///
  /// The caller should clear @p out before calling if reusing the vector.
  ///
  /// @param min_events  Minimum completions to wait for. Must be >= 1.
  /// @param max_events  Maximum completions to return. Must be >= min_events.
  /// @param[out] out    Vector to append completed IO operations to.
  virtual void PollCompletions(int min_events, int max_events,
                               std::vector<IOCompletion>& out) = 0;

  /// @brief Release all resources (file descriptor, kernel context).
  ///
  /// Safe to call if Open() was never called or already closed.
  virtual void Close() = 0;

  /// @brief Check whether O_DIRECT is active after Open().
  /// @return true if the file was opened with O_DIRECT, false if fallback
  ///         to buffered IO occurred.
  virtual bool IsDirectEnabled() const noexcept = 0;
};

}  // namespace cfio

#endif  // CFIO_ENGINE_I_ENGINE_IO_H_
