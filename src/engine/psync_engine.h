#ifndef CFIO_ENGINE_PSYNC_ENGINE_H_
#define CFIO_ENGINE_PSYNC_ENGINE_H_

/// @file psync_engine.h
/// @brief Synchronous positional IO engine using pread/pwrite.

#include "common/types.h"
#include "engine/i_engine_io.h"

namespace cfio {

/// @brief Synchronous IO engine that uses pread and pwrite.
class PsyncEngine : public IEngineIO {
 public:
  PsyncEngine() = default;
  ~PsyncEngine() override;

  PsyncEngine(const PsyncEngine&) = delete;
  PsyncEngine& operator=(const PsyncEngine&) = delete;
  PsyncEngine(PsyncEngine&&) = delete;
  PsyncEngine& operator=(PsyncEngine&&) = delete;

  void Open(const JobConfig& config) override;
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

#endif  // CFIO_ENGINE_PSYNC_ENGINE_H_
