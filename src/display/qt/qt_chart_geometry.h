#ifndef CFIO_DISPLAY_QT_QT_CHART_GEOMETRY_H_
#define CFIO_DISPLAY_QT_QT_CHART_GEOMETRY_H_

/// @file qt_chart_geometry.h
/// @brief Qt free chart maths for the time series graphs

#include <cstdint>
#include <string>
#include <vector>

#include "telemetry/metrics_snapshot.h"

namespace cfio {

constexpr int kDefaultTickCount = 5;

/// @brief Metric drawn on a chart
enum class ChartMetric : std::uint8_t {
  kIops,        ///< Instant IOPS
  kBandwidth,   ///< Instant bandwidth in bytes per second
  kLatencyP50,  ///< Median latency in nanoseconds
  kLatencyP95,  ///< 95th percentile latency in nanoseconds
  kLatencyP99   ///< 99th percentile latency in nanoseconds
};

/// @brief One point in either value space or pixel space
struct ChartPoint {
  double x{};  ///< Elapsed seconds, or pixel column
  double y{};  ///< Metric value, or pixel row
};

/// @brief Named sequence of points
struct ChartSeries {
  std::string label;               ///< Legend text
  std::vector<ChartPoint> points;  ///< Samples in time order
};

/// @brief Rounded value range of one axis
struct AxisScale {
  double min{};         ///< Lowest tick value
  double max{1.0};      ///< Highest tick value
  double tick_step{1};  ///< Distance between ticks
};

/// @brief Pixel area the plot is drawn into
struct ChartRect {
  double left{};       ///< Left edge in pixels
  double top{};        ///< Top edge in pixels
  double width{1.0};   ///< Width in pixels
  double height{1.0};  ///< Height in pixels
};

/// @brief Pick a round tick distance of 1, 2 or 5 times a power of ten
///
/// @param range         Span the axis has to cover, non positive values give 1.
/// @param target_ticks  Wanted number of intervals, values below one give the default.
/// @return The chosen tick distance, always greater than zero.
[[nodiscard]] double NiceTickStep(double range, int target_ticks);

/// @brief Round a value range outwards to whole ticks
///
/// @param min_value     Lowest data value.
/// @param max_value     Highest data value.
/// @param target_ticks  Wanted number of intervals, values below one give the default.
/// @return A scale whose bounds are multiples of the tick step and where max is above min.
[[nodiscard]] AxisScale MakeAxisScale(double min_value, double max_value,
                                      int target_ticks = kDefaultTickCount);

/// @brief List every tick value of a scale
///
/// @param scale  Scale to walk.
/// @return Tick values from min to max inclusive.
[[nodiscard]] std::vector<double> AxisTicks(const AxisScale& scale);

/// @brief Map a value to a pixel column
///
/// @param value  Value on the horizontal axis.
/// @param scale  Horizontal scale.
/// @param plot   Plot area in pixels.
/// @return The pixel column, clamped to the plot area.
[[nodiscard]] double MapX(double value, const AxisScale& scale, const ChartRect& plot);

/// @brief Map a value to a pixel row
///
/// @param value  Value on the vertical axis.
/// @param scale  Vertical scale.
/// @param plot   Plot area in pixels.
/// @return The pixel row, clamped to the plot area and growing downwards.
[[nodiscard]] double MapY(double value, const AxisScale& scale, const ChartRect& plot);

/// @brief Map a whole series into pixel space
///
/// @param points  Points in value space.
/// @param x       Horizontal scale.
/// @param y       Vertical scale.
/// @param plot    Plot area in pixels.
/// @return The same points expressed in pixels.
[[nodiscard]] std::vector<ChartPoint> MapPoints(const std::vector<ChartPoint>& points,
                                                const AxisScale& x, const AxisScale& y,
                                                const ChartRect& plot);

/// @brief Read one metric of the aggregate row out of a snapshot history
///
/// @param history  Samples in time order.
/// @param metric   Metric to read.
/// @return A series whose x is seconds since the first sample, empty if history is empty.
[[nodiscard]] ChartSeries ExtractSeries(const std::vector<MetricsSnapshot>& history,
                                        ChartMetric metric);

/// @brief Find the elapsed time covered by a snapshot history
///
/// @param history  Samples in time order.
/// @return Seconds between the first and the last sample, 0 if there are fewer than two.
[[nodiscard]] double SeriesDurationSeconds(const std::vector<MetricsSnapshot>& history);

/// @brief Find the largest y over several series
///
/// @param series  Series to scan.
/// @return The highest y value, 0 if every series is empty.
[[nodiscard]] double MaxY(const std::vector<ChartSeries>& series);

}  // namespace cfio

#endif  // CFIO_DISPLAY_QT_QT_CHART_GEOMETRY_H_
