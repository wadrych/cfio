#ifndef CFIO_DISPLAY_I_DISPLAY_H_
#define CFIO_DISPLAY_I_DISPLAY_H_

/// @file i_display.h
/// @brief Abstract interface for all UI backends.

#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {

/// @brief Abstract interface for rendering benchmark progress and results
///
class IDisplay {
 public:
  IDisplay() = default;
  virtual ~IDisplay() = default;

  IDisplay(const IDisplay&) = delete;
  IDisplay& operator=(const IDisplay&) = delete;
  IDisplay(IDisplay&&) = delete;
  IDisplay& operator=(IDisplay&&) = delete;

  /// @brief Prepare the display
  /// @param runtime_seconds  Total run duration used to render progress
  virtual void Init(int runtime_seconds) = 0;

  /// @brief Refresh the live view
  /// @param snapshot Recent sampled metrics
  virtual void Update(const MetricsSnapshot& snapshot) = 0;

  /// @brief Render the final results
  /// @param results  Full result set
  virtual void ShowSummary(const BenchmarkResults& results) = 0;

  /// @brief Release resources and restore terminal state.
  virtual void Shutdown() = 0;
};

}  // namespace cfio

#endif  // CFIO_DISPLAY_I_DISPLAY_H_
