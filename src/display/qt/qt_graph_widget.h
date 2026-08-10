#ifndef CFIO_DISPLAY_QT_QT_GRAPH_WIDGET_H_
#define CFIO_DISPLAY_QT_QT_GRAPH_WIDGET_H_

/// @file qt_graph_widget.h
/// @brief Time series sub plot drawn with QPainter

#include <vector>

#include <QSize>
#include <QWidget>

#include "display/qt/qt_chart_geometry.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {

/// @brief One line chart showing the metrics of a single sub plot over time
class QtGraphWidget final : public QWidget {
  Q_OBJECT

 public:
  /// @brief Build an empty chart
  ///
  /// @param kind    Metrics this chart draws.
  /// @param parent  Owning widget, may be null.
  explicit QtGraphWidget(ChartKind kind, QWidget* parent = nullptr);

  /// @brief Replace the plotted data and repaint
  /// @param history  Metrics samples in time order, may be empty.
  void SetSeries(const std::vector<MetricsSnapshot>& history);

  /// @brief Smallest useful size of the chart
  /// @return Size that still fits the axes and their labels.
  [[nodiscard]] QSize minimumSizeHint() const override;

 protected:
  /// @brief Draw the background, grid, axes, labels and lines
  /// @param event  Paint event, unused.
  void paintEvent(QPaintEvent* event) override;

 private:
  ChartKind kind_;                   ///< Metrics this chart draws
  std::vector<ChartSeries> series_;  ///< One entry per metric of the kind
  AxisScale x_scale_;                ///< Elapsed time scale in seconds
  AxisScale y_scale_;                ///< Value scale in the unit of the kind
  bool has_data_{};                  ///< True once a non empty history arrived
};

}  // namespace cfio

#endif  // CFIO_DISPLAY_QT_QT_GRAPH_WIDGET_H_
