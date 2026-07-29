#ifndef CFIO_ENGINE_SYNC_ENGINE_H_
#define CFIO_ENGINE_SYNC_ENGINE_H_

/// @file sync_engine.h
/// @brief Synchronous IO engine using lseek + read/write.

#include "common/types.h"
#include "engine/i_engine_io.h"

namespace cfio {

/// @brief Synchronous IO engine that uses lseek followed by read or write.
class SyncEngine final : public IEngineIO {
 public:
  SyncEngine() = default;
  ~SyncEngine() override;

  SyncEngine(const SyncEngine&) = delete;
  SyncEngine& operator=(const SyncEngine&) = delete;
  SyncEngine(SyncEngine&&) = delete;
  SyncEngine& operator=(SyncEngine&&) = delete;

  void Open(const JobConfig& config) override;

  /// @brief Execute a single IO using lseek + read or write.
  ///
  /// @param request  The IO operation to execute.
  /// @throws std::logic_error if previous completion was not polled.
  /// @throws std::runtime_error if the engine is not open.
  void SubmitIO(const IORequest& request) override;

  void PollCompletions(int min_events, int max_events, std::vector<IOCompletion>& out) override;
  void Close() override;
  [[nodiscard]] bool IsDirectEnabled() const noexcept override;

 private:
  int fd_ = -1;
  bool direct_effective_ = false;
  bool has_completion_ = false;
  IOCompletion last_completion_ = {};
};

}  // namespace cfio

#endif  // CFIO_ENGINE_SYNC_ENGINE_H_
