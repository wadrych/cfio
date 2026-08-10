/// @file qt_chart_geometry.cpp
/// @brief Implementation of the Qt free chart maths

#include "display/qt/qt_chart_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "display/metric_format.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

constexpr int kMaxTicks = 64;
constexpr double kTickEpsilon = 1e-9;
constexpr double kMinPlotSize = 1.0;

std::uint64_t TickToUnsigned(double value) {
  if (!std::isfinite(value) || value <= 0.0) {
    return 0;
  }
  return static_cast<std::uint64_t>(std::llround(value));
}

double MetricValue(const PerJobMetrics& metrics, ChartMetric metric) {
  switch (metric) {
    case ChartMetric::kIops:
      return static_cast<double>(metrics.iops_instant);
    case ChartMetric::kBandwidth:
      return static_cast<double>(metrics.bw_instant);
    case ChartMetric::kLatencyP50:
      return static_cast<double>(metrics.lat_p50_ns);
    case ChartMetric::kLatencyP95:
      return static_cast<double>(metrics.lat_p95_ns);
    case ChartMetric::kLatencyP99:
      return static_cast<double>(metrics.lat_p99_ns);
  }
  return 0.0;
}

std::string MetricLabel(ChartMetric metric) {
  switch (metric) {
    case ChartMetric::kIops:
      return "IOPS";
    case ChartMetric::kBandwidth:
      return "Bandwidth";
    case ChartMetric::kLatencyP50:
      return "p50";
    case ChartMetric::kLatencyP95:
      return "p95";
    case ChartMetric::kLatencyP99:
      return "p99";
  }
  return "";
}

}  // namespace

std::vector<ChartMetric> ChartKindMetrics(ChartKind kind) {
  switch (kind) {
    case ChartKind::kIops:
      return {ChartMetric::kIops};
    case ChartKind::kBandwidth:
      return {ChartMetric::kBandwidth};
    case ChartKind::kLatency:
      return {ChartMetric::kLatencyP50, ChartMetric::kLatencyP95, ChartMetric::kLatencyP99};
  }
  return {};
}

std::string ChartKindTitle(ChartKind kind) {
  switch (kind) {
    case ChartKind::kIops:
      return "IOPS";
    case ChartKind::kBandwidth:
      return "Bandwidth";
    case ChartKind::kLatency:
      return "Latency";
  }
  return "";
}

std::string FormatAxisTick(ChartKind kind, double value) {
  const std::uint64_t rounded = TickToUnsigned(value);
  switch (kind) {
    case ChartKind::kIops:
      return FormatCount(rounded);
    case ChartKind::kBandwidth:
      return FormatRate(rounded);
    case ChartKind::kLatency:
      return FormatLatencyUs(rounded);
  }
  return "";
}

AxisScale MakeUnitAxisScale(ChartKind kind, double min_value, double max_value) {
  double divisor = 1.0;
  switch (kind) {
    case ChartKind::kIops:
      break;
    case ChartKind::kBandwidth:
      divisor = static_cast<double>(kBytesPerMiB);
      break;
    case ChartKind::kLatency:
      divisor = static_cast<double>(kNsPerUs);
      break;
  }

  AxisScale scale = MakeAxisScale(min_value / divisor, max_value / divisor);
  if (scale.tick_step < 1.0) {
    // Labels are whole drawn units, a finer step would repeat the same text on every tick.
    scale.tick_step = 1.0;
    scale.min = std::floor(scale.min);
    scale.max = std::max(std::ceil(scale.max), scale.min + 1.0);
  }
  scale.min *= divisor;
  scale.max *= divisor;
  scale.tick_step *= divisor;
  return scale;
}

std::string FormatTimeTick(double seconds) {
  if (!std::isfinite(seconds) || seconds <= 0.0) {
    return FormatDuration(0);
  }
  return FormatDuration(static_cast<int>(std::llround(seconds)));
}

ChartRect ComputePlotRect(double width, double height, const ChartMargins& margins) {
  ChartRect plot;
  plot.left = margins.left;
  plot.top = margins.top;
  plot.width = std::max(width - margins.left - margins.right, kMinPlotSize);
  plot.height = std::max(height - margins.top - margins.bottom, kMinPlotSize);
  return plot;
}

double NiceTickStep(double range, int target_ticks) {
  if (!std::isfinite(range) || range <= 0.0) {
    return 1.0;
  }
  const int ticks = target_ticks > 0 ? target_ticks : kDefaultTickCount;

  const double raw = range / static_cast<double>(ticks);
  const double exponent = std::floor(std::log10(raw));
  const double power = std::pow(10.0, exponent);
  const double fraction = raw / power;

  double nice = 10.0;
  if (fraction <= 1.0) {
    nice = 1.0;
  } else if (fraction <= 2.0) {
    nice = 2.0;
  } else if (fraction <= 5.0) {
    nice = 5.0;
  }
  return nice * power;
}

AxisScale MakeAxisScale(double min_value, double max_value, int target_ticks) {
  double low = min_value;
  double high = max_value;
  if (!std::isfinite(low) || !std::isfinite(high)) {
    low = 0.0;
    high = 0.0;
  }
  if (high < low) {
    std::swap(low, high);
  }

  if (high <= low) {
    low = std::min(low, 0.0);
    high = high > 0.0 ? high : 1.0;
  }

  const double step = NiceTickStep(high - low, target_ticks);
  AxisScale scale;
  scale.tick_step = step;
  scale.min = std::floor(low / step) * step;
  scale.max = std::ceil(high / step) * step;
  if (scale.max - scale.min < step) {
    scale.max = scale.min + step;
  }
  return scale;
}

std::vector<double> AxisTicks(const AxisScale& scale) {
  std::vector<double> ticks;
  if (scale.tick_step <= 0.0 || scale.max < scale.min) {
    return ticks;
  }

  const double span = scale.max - scale.min;
  const auto count = static_cast<int>(std::floor((span / scale.tick_step) + kTickEpsilon));
  const int limit = std::min(count, kMaxTicks);
  ticks.reserve(static_cast<std::size_t>(limit) + 1);
  for (int i = 0; i <= limit; ++i) {
    ticks.push_back(scale.min + (static_cast<double>(i) * scale.tick_step));
  }
  return ticks;
}

double MapX(double value, const AxisScale& scale, const ChartRect& plot) {
  const double span = scale.max - scale.min;
  if (span <= 0.0) {
    return plot.left;
  }
  const double ratio = std::clamp((value - scale.min) / span, 0.0, 1.0);
  return plot.left + (ratio * plot.width);
}

double MapY(double value, const AxisScale& scale, const ChartRect& plot) {
  const double span = scale.max - scale.min;
  if (span <= 0.0) {
    return plot.top + plot.height;
  }
  const double ratio = std::clamp((value - scale.min) / span, 0.0, 1.0);
  return plot.top + plot.height - (ratio * plot.height);
}

std::vector<ChartPoint> MapPoints(const std::vector<ChartPoint>& points, const AxisScale& x,
                                  const AxisScale& y, const ChartRect& plot) {
  std::vector<ChartPoint> mapped;
  mapped.reserve(points.size());
  for (const ChartPoint& point : points) {
    mapped.push_back(ChartPoint{MapX(point.x, x, plot), MapY(point.y, y, plot)});
  }
  return mapped;
}

ChartSeries ExtractSeries(const std::vector<MetricsSnapshot>& history, ChartMetric metric) {
  ChartSeries series;
  series.label = MetricLabel(metric);
  if (history.empty()) {
    return series;
  }

  series.points.reserve(history.size() + 1);
  if (history.front().elapsed_seconds > 0.0) {
    series.points.push_back(ChartPoint{0.0, 0.0});
  }
  for (const MetricsSnapshot& snapshot : history) {
    series.points.push_back(
        ChartPoint{snapshot.elapsed_seconds, MetricValue(snapshot.aggregate, metric)});
  }
  return series;
}

double SeriesDurationSeconds(const std::vector<MetricsSnapshot>& history) {
  if (history.empty()) {
    return 0.0;
  }
  return std::max(std::round(history.back().elapsed_seconds), 0.0);
}

double MaxY(const std::vector<ChartSeries>& series) {
  double highest = 0.0;
  for (const ChartSeries& one : series) {
    for (const ChartPoint& point : one.points) {
      highest = std::max(highest, point.y);
    }
  }
  return highest;
}

}  // namespace cfio
