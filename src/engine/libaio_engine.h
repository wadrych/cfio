#ifndef CFIO_ENGINE_LIBAIO_ENGINE_H_
#define CFIO_ENGINE_LIBAIO_ENGINE_H_

/// @file libaio_engine.h
/// @brief Asynchronous IO engine using Linux AIO via libaio.

#include <libaio.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "common/types.h"
#include "engine/i_engine_io.h"
#include "engine/submit_info.h"

namespace cfio {

/// @brief Asynchronous IO engine using the Linux AIO kernel interface.
class LibaioEngine : public IEngineIO {
 public:
  LibaioEngine() = default;
  ~LibaioEngine() override;

  LibaioEngine(const LibaioEngine&) = delete;
  LibaioEngine& operator=(const LibaioEngine&) = delete;
  LibaioEngine(LibaioEngine&&) = delete;
  LibaioEngine& operator=(LibaioEngine&&) = delete;

  void Open(const JobConfig& config) override;

  /// @brief Submit a single IO to the kernel AIO context.
  ///
  /// @param request  The IO operation to submit. Buffer must stay valid until
  ///                 the matching completion is returned from PollCompletions.
  /// @throws std::logic_error if iocb pool is empty or request.id is duplicate.
  /// @throws std::system_error if io_submit fails.
  /// @throws std::runtime_error if the engine is not open.
  void SubmitIO(const IORequest& request) override;

  void PollCompletions(int min_events, int max_events, std::vector<IOCompletion>& out) override;
  void Close() override;
  [[nodiscard]] bool IsDirectEnabled() const noexcept override;

 private:
  io_context_t ctx_ = nullptr;
  int fd_ = -1;
  bool direct_effective_ = false;
  int iodepth_ = 0;

  std::vector<struct iocb> iocb_pool_;
  std::vector<struct iocb*> free_iocbs_;
  std::vector<struct io_event> events_;
  std::unordered_map<uint64_t, SubmitInfo> in_flight_;
};

}  // namespace cfio

#endif  // CFIO_ENGINE_LIBAIO_ENGINE_H_
