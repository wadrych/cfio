#ifndef CFIO_DISPLAY_TERMINAL_DISPLAY_H_
#define CFIO_DISPLAY_TERMINAL_DISPLAY_H_

/// @file terminal_display.h
/// @brief Text terminal backend rendering the live metrics view

#include <chrono>
#include <iosfwd>
#include <string>

#include "display/display_context.h"
#include "display/i_display.h"

namespace cfio {

/// @brief Renders benchmark metrics as an in place ANSI table.
class TerminalDisplay final : public IDisplay {
 public:
  /// @brief Construct a terminal display
  /// @param context  Run metadata
  explicit TerminalDisplay(DisplayContext context);

  /// @brief Construct a terminal display writing to a given stream
  /// @param context  Run metadata
  /// @param out      Sink for rendered frames
  TerminalDisplay(DisplayContext context, std::ostream& out);

  TerminalDisplay(const TerminalDisplay&) = delete;
  TerminalDisplay& operator=(const TerminalDisplay&) = delete;
  TerminalDisplay(TerminalDisplay&&) = delete;
  TerminalDisplay& operator=(TerminalDisplay&&) = delete;

  ~TerminalDisplay() override = default;

  /// @brief Clear the screen, hide the cursor and paint the initial frame
  /// @param runtime_seconds  Total run duration
  void Init(int runtime_seconds) override;

  /// @brief Redraw the live table from the current snapshot
  /// @param snapshot  Latest sample
  void Update(const MetricsSnapshot& snapshot) override;

  /// @brief Post run summary
  void ShowSummary(const BenchmarkResults& results) override;

  /// @brief Restore the cursor and leave it below the frame
  void Shutdown() override;

  /// @brief Build the full live frame as a string
  /// @param snapshot         Metrics to render
  /// @param elapsed_seconds  Elapsed time for the progress indicator
  [[nodiscard]] std::string RenderLiveView(const MetricsSnapshot& snapshot,
                                           int elapsed_seconds) const;

  /// @brief Build the summary
  /// @param results  Final results
  [[nodiscard]] std::string RenderSummary(const BenchmarkResults& results) const;

 private:
  DisplayContext context_;
  std::ostream* out_;
  int runtime_seconds_ = 0;
  std::chrono::steady_clock::time_point start_;
};

}  // namespace cfio

#endif  // CFIO_DISPLAY_TERMINAL_DISPLAY_H_
