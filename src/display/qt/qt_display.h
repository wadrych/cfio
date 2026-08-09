#ifndef CFIO_DISPLAY_QT_QT_DISPLAY_H_
#define CFIO_DISPLAY_QT_QT_DISPLAY_H_

/// @file qt_display.h
/// @brief Display backend forwarding the run lifecycle into the GUI mailbox

#include "display/display_context.h"
#include "display/i_display.h"
#include "display/qt/run_mailbox.h"

namespace cfio {

/// @brief Writes benchmark progress into a RunMailbox for the GUI thread to poll.
class QtDisplay final : public IDisplay {
 public:
  /// @brief Construct a display bound to a mailbox
  /// @param mailbox  Handoff cell read by the GUI thread, must outlive this display
  /// @param context  Run metadata shown in the window header
  QtDisplay(RunMailbox& mailbox, DisplayContext context);

  ~QtDisplay() override = default;

  QtDisplay(const QtDisplay&) = delete;
  QtDisplay& operator=(const QtDisplay&) = delete;
  QtDisplay(QtDisplay&&) = delete;
  QtDisplay& operator=(QtDisplay&&) = delete;

  /// @brief Record the run duration and enter the running phase
  /// @param runtime_seconds  Total run duration
  void Init(int runtime_seconds) override;

  /// @brief Publish the newest sample
  /// @param snapshot  Latest sample
  void Update(const MetricsSnapshot& snapshot) override;

  /// @brief Publish the final results and enter the finished phase
  /// @param results  Final results
  void ShowSummary(const BenchmarkResults& results) override;

  /// @brief Enter the finished phase without touching published results
  void Shutdown() override;

  /// @brief Check whether the GUI asked for an early stop
  /// @return true once the GUI called RequestStop on the mailbox
  [[nodiscard]] bool StopRequested() const override;

  /// @brief Read the run metadata
  /// @return Metadata passed at construction
  [[nodiscard]] const DisplayContext& Context() const;

 private:
  RunMailbox* mailbox_;     ///< Handoff cell, never null
  DisplayContext context_;  ///< Run metadata for the window header
};

}  // namespace cfio

#endif  // CFIO_DISPLAY_QT_QT_DISPLAY_H_
