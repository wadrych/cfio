#ifndef CFIO_DISPLAY_QT_QT_JOB_TABLE_WIDGET_H_
#define CFIO_DISPLAY_QT_QT_JOB_TABLE_WIDGET_H_

/// @file qt_job_table_widget.h
/// @brief Live metrics table widget

#include <QTableWidget>
#include <QWidget>

#include "telemetry/metrics_snapshot.h"

namespace cfio {

/// @brief Table showing one row per job plus the aggregate row
class QtJobTableWidget final : public QTableWidget {
  Q_OBJECT

 public:
  /// @brief Build an empty table with the column headers in place
  ///
  /// @param parent  Owning widget, may be null.
  explicit QtJobTableWidget(QWidget* parent = nullptr);

  /// @brief Refresh every cell from a metrics sample
  /// @param snapshot  Sample to render.
  void SetSnapshot(const MetricsSnapshot& snapshot);
};

}  // namespace cfio

#endif  // CFIO_DISPLAY_QT_QT_JOB_TABLE_WIDGET_H_
