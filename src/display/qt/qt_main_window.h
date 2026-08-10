#ifndef CFIO_DISPLAY_QT_QT_MAIN_WINDOW_H_
#define CFIO_DISPLAY_QT_QT_MAIN_WINDOW_H_

/// @file qt_main_window.h
/// @brief Main GUI window polling the run mailbox on a timer

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <QCloseEvent>
#include <QElapsedTimer>
#include <QLabel>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

#include "display/display_context.h"
#include "display/qt/qt_graph_widget.h"
#include "display/qt/qt_job_table_widget.h"
#include "display/qt/run_mailbox.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {

/// @brief Number of stacked sub plots below the metrics table
constexpr std::size_t kGraphCount = 3;

/// @brief Window showing the live metrics of a run and its final results
class QtMainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  /// @brief Build the window and start polling the mailbox
  ///
  /// @param mailbox  Handoff cell written by the benchmark thread, must outlive this window.
  /// @param context  Run metadata shown in the header.
  /// @param parent   Owning widget, may be null.
  QtMainWindow(RunMailbox& mailbox, DisplayContext context, QWidget* parent = nullptr);

 protected:
  /// @brief Ask the benchmark thread to stop, then let the window close
  /// @param event  Close event, always accepted.
  void closeEvent(QCloseEvent* event) override;

 private:
  /// @brief Build the header, progress row, table and sub plots
  void BuildLayout();

  /// @brief Poll the mailbox and refresh whatever changed
  void OnTick();

  /// @brief Stop the run, or close the window once the run is over
  void OnStopClicked();

  /// @brief Render one sample into the table and the sub plots
  /// @param snapshot  Sample to show.
  void ApplySnapshot(const MetricsSnapshot& snapshot);

  /// @brief Replace the live history with the final results
  /// @param results  Final result set.
  void ApplyResults(const BenchmarkResults& results);

  /// @brief Leave the running state, drop the progress bar and turn stop into close
  void MarkFinished();

  /// @brief Move the progress bar and the clock
  void UpdateProgress();

  RunMailbox* mailbox_;                   ///< Handoff cell, never null
  DisplayContext context_;                ///< Run metadata for the header
  std::vector<MetricsSnapshot> history_;  ///< Samples fed to the sub plots
  std::uint64_t last_sequence_{0};        ///< Mailbox sequence already rendered
  QElapsedTimer clock_;                   ///< Started when the run enters the running phase
  bool finished_{false};                  ///< True once the run is over

  QTimer* timer_;                                     ///< Drives the mailbox polling
  QProgressBar* progress_;                            ///< Elapsed fraction of the runtime
  QLabel* status_;                                    ///< Clock while running, totals when done
  QPushButton* stop_button_;                          ///< Stops the run, then closes the window
  QtJobTableWidget* table_;                           ///< Per job metrics
  std::array<QtGraphWidget*, kGraphCount> graphs_{};  ///< IOPS, bandwidth and latency sub plots
};

}  // namespace cfio

#endif  // CFIO_DISPLAY_QT_QT_MAIN_WINDOW_H_
