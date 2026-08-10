/// @file qt_graph_widget.cpp
/// @brief Implementation of the QPainter time series sub plot

#include "display/qt/qt_graph_widget.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <Qt>

#include "display/qt/qt_chart_geometry.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

constexpr std::array<QRgb, 3> kSeriesColors = {0x3D8BFDU, 0xE8A33DU, 0xC0392BU};

constexpr int kMinWidth = 320;
constexpr int kMinHeight = 140;
constexpr double kLabelPadding = 6.0;
constexpr double kRightMargin = 12.0;
constexpr double kTitleGap = 4.0;
constexpr double kLinePenWidth = 2.0;
constexpr double kLegendSwatch = 14.0;
constexpr double kLegendGap = 16.0;
constexpr int kGridAlpha = 60;
constexpr int kAxisAlpha = 140;

QColor SeriesColor(std::size_t index) {
  return {kSeriesColors.at(index % kSeriesColors.size())};
}

}  // namespace

QtGraphWidget::QtGraphWidget(ChartKind kind, QWidget* parent) : QWidget(parent), kind_(kind) {
  setAutoFillBackground(false);
}

void QtGraphWidget::SetSeries(const std::vector<MetricsSnapshot>& history) {
  series_.clear();
  has_data_ = !history.empty();

  for (const ChartMetric metric : ChartKindMetrics(kind_)) {
    series_.push_back(ExtractSeries(history, metric));
  }

  x_scale_ = MakeAxisScale(0.0, SeriesDurationSeconds(history));
  y_scale_ = MakeUnitAxisScale(kind_, 0.0, MaxY(series_));
  update();
}

QSize QtGraphWidget::minimumSizeHint() const {
  return {kMinWidth, kMinHeight};
}

void QtGraphWidget::paintEvent(QPaintEvent* /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.fillRect(rect(), palette().base());

  const QFontMetricsF metrics(font());
  const QColor text_color = palette().text().color();

  painter.setPen(QPen(text_color));
  painter.drawText(QPointF(kLabelPadding, metrics.ascent() + kLabelPadding),
                   QString::fromStdString(ChartKindTitle(kind_)));

  if (!has_data_) {
    painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("waiting for data"));
    return;
  }

  const std::vector<double> y_ticks = AxisTicks(y_scale_);
  const std::vector<double> x_ticks = AxisTicks(x_scale_);

  std::vector<QString> y_labels;
  y_labels.reserve(y_ticks.size());
  double widest_label = 0.0;
  for (const double tick : y_ticks) {
    QString label = QString::fromStdString(FormatAxisTick(kind_, tick));
    widest_label = qMax(widest_label, metrics.horizontalAdvance(label));
    y_labels.push_back(std::move(label));
  }

  double last_time_label = 0.0;
  if (!x_ticks.empty()) {
    last_time_label =
        metrics.horizontalAdvance(QString::fromStdString(FormatTimeTick(x_ticks.back())));
  }

  const bool has_legend = series_.size() > 1;
  ChartMargins margins;
  margins.left = widest_label + (2.0 * kLabelPadding);
  margins.top = metrics.height() + kLabelPadding + kTitleGap +
                (has_legend ? metrics.height() + kTitleGap : 0.0);
  margins.right = qMax(kRightMargin, (last_time_label / 2.0) + kLabelPadding);
  margins.bottom = metrics.height() + (2.0 * kLabelPadding);

  const ChartRect plot = ComputePlotRect(width(), height(), margins);

  if (has_legend) {
    double legend_x = margins.left;
    const double legend_y = metrics.height() + kLabelPadding + kTitleGap + (metrics.height() / 2.0);
    for (std::size_t i = 0; i < series_.size(); ++i) {
      painter.setPen(QPen(SeriesColor(i), kLinePenWidth));
      painter.drawLine(QPointF(legend_x, legend_y), QPointF(legend_x + kLegendSwatch, legend_y));

      const QString label = QString::fromStdString(series_[i].label);
      const double text_x = legend_x + kLegendSwatch + kLabelPadding;
      painter.setPen(QPen(text_color));
      painter.drawText(QPointF(text_x, legend_y + (metrics.ascent() / 2.0)), label);
      legend_x = text_x + metrics.horizontalAdvance(label) + kLegendGap;
    }
  }

  QColor grid_color = text_color;
  grid_color.setAlpha(kGridAlpha);
  QColor axis_color = text_color;
  axis_color.setAlpha(kAxisAlpha);

  for (std::size_t i = 0; i < y_ticks.size(); ++i) {
    const double row = MapY(y_ticks[i], y_scale_, plot);
    painter.setPen(QPen(grid_color));
    painter.drawLine(QPointF(plot.left, row), QPointF(plot.left + plot.width, row));

    painter.setPen(QPen(text_color));
    const QRectF label_box(0.0, row - (metrics.height() / 2.0), plot.left - kLabelPadding,
                           metrics.height());
    painter.drawText(label_box, Qt::AlignRight | Qt::AlignVCenter, y_labels[i]);
  }

  for (const double tick : x_ticks) {
    const double column = MapX(tick, x_scale_, plot);
    painter.setPen(QPen(grid_color));
    painter.drawLine(QPointF(column, plot.top), QPointF(column, plot.top + plot.height));

    const QString label = QString::fromStdString(FormatTimeTick(tick));
    const double label_width = metrics.horizontalAdvance(label);
    const QRectF label_box(column - (label_width / 2.0), plot.top + plot.height + kLabelPadding,
                           label_width, metrics.height());
    painter.setPen(QPen(text_color));
    painter.drawText(label_box, Qt::AlignHCenter | Qt::AlignTop, label);
  }

  painter.setPen(QPen(axis_color));
  painter.drawLine(QPointF(plot.left, plot.top), QPointF(plot.left, plot.top + plot.height));
  painter.drawLine(QPointF(plot.left, plot.top + plot.height),
                   QPointF(plot.left + plot.width, plot.top + plot.height));

  for (std::size_t i = 0; i < series_.size(); ++i) {
    const std::vector<ChartPoint> mapped = MapPoints(series_[i].points, x_scale_, y_scale_, plot);
    QPolygonF line;
    line.reserve(static_cast<int>(mapped.size()));
    for (const ChartPoint& point : mapped) {
      line << QPointF(point.x, point.y);
    }

    painter.setPen(QPen(SeriesColor(i), kLinePenWidth));
    if (line.size() == 1) {
      painter.drawPoint(line.front());
    } else {
      painter.drawPolyline(line);
    }
  }
}

}  // namespace cfio
