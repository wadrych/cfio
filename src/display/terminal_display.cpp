/// @file terminal_display.cpp
/// @brief Live metrics rendering for the text terminal

#include "display/terminal_display.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "config/job_config.h"
#include "config/size_parser.h"
#include "display/display_context.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

// ANSI control sequences.
constexpr std::string_view kClearScreen = "\033[2J";
constexpr std::string_view kCursorHome = "\033[H";
constexpr std::string_view kHideCursor = "\033[?25l";
constexpr std::string_view kShowCursor = "\033[?25h";
constexpr std::string_view kClearLine = "\033[K";
constexpr std::string_view kClearBelow = "\033[0J";

// Box drawing and bar glyphs, pinned by code point
constexpr std::string_view kVert = "│";      // vertical rule
constexpr std::string_view kCross = "┼";     // rule junction
constexpr std::string_view kHoriz = "─";     // light horizontal rule
constexpr std::string_view kDouble = "═";    // heavy horizontal rule
constexpr std::string_view kBarFull = "█";   // progress filled cell
constexpr std::string_view kBarEmpty = "░";  // progress empty cell
constexpr std::string_view kDash = "—";      // header em dash
constexpr std::string_view kMicro = "μ";     // micro sign for latency

// Column content widths
constexpr int kColJob = 16;
constexpr int kColIops = 19;
constexpr int kColBw = 18;
constexpr int kColLat = 16;

// Full row width
constexpr int kRowWidth = 1 + kColJob + 3 + kColIops + 3 + kColBw + 3 + kColLat;

constexpr int kBarWidth = 40;
constexpr std::uint64_t kBytesPerKiB = 1024ULL;
constexpr std::uint64_t kBytesPerMiB = 1024ULL * 1024ULL;
constexpr std::uint64_t kBytesPerGiB = 1024ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kNsPerUs = 1000ULL;
constexpr int kSecPerMin = 60;

// Repeat a multibyte glyph count times
std::string RepeatGlyph(std::string_view glyph, int count) {
  std::string out;
  if (count <= 0) {
    return out;
  }
  out.reserve(glyph.size() * static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    out.append(glyph);
  }
  return out;
}

std::string FormatCount(std::uint64_t value) {
  const std::string digits = std::to_string(value);
  const std::size_t count = digits.size();
  std::string out;
  out.reserve(count + ((count - 1) / 3));
  for (std::size_t i = 0; i < count; ++i) {
    if (i != 0 && (count - i) % 3 == 0) {
      out.push_back(',');
    }
    out.push_back(digits[i]);
  }
  return out;
}

std::string FormatDuration(int seconds) {
  const int clamped = std::max(0, seconds);
  return fmt::format("{:02}:{:02}", clamped / kSecPerMin, clamped % kSecPerMin);
}

std::string FormatProgressBar(int elapsed, int total, int width) {
  double frac = 0.0;
  if (total > 0) {
    frac = std::clamp(static_cast<double>(elapsed) / static_cast<double>(total), 0.0, 1.0);
  }
  const int filled = static_cast<int>(frac * static_cast<double>(width));
  const int pct = static_cast<int>(std::lround(frac * 100.0));
  return fmt::format("[{}{}] {:3}%", RepeatGlyph(kBarFull, filled),
                     RepeatGlyph(kBarEmpty, width - filled), pct);
}

std::string FormatIopsCell(std::uint64_t instant, std::uint64_t average) {
  return fmt::format("{} / {}", FormatCount(instant), FormatCount(average));
}

std::string FormatBwCell(std::uint64_t instant, std::uint64_t average) {
  return fmt::format("{}/{} MB/s", instant / kBytesPerMiB, average / kBytesPerMiB);
}

std::string FormatLatCell(std::uint64_t p50, std::uint64_t p95, std::uint64_t p99) {
  return fmt::format("{}/{}/{} {}s", p50 / kNsPerUs, p95 / kNsPerUs, p99 / kNsPerUs, kMicro);
}

std::string DataRow(std::string_view job, std::string_view iops, std::string_view bw,
                    std::string_view lat) {
  return fmt::format(" {:<{}} {} {:<{}} {} {:<{}} {} {}", job, kColJob, kVert, iops, kColIops,
                     kVert, bw, kColBw, kVert, lat);
}

std::string SeparatorRow() {
  return RepeatGlyph(kHoriz, kColJob + 2) + std::string(kCross) +
         RepeatGlyph(kHoriz, kColIops + 2) + std::string(kCross) + RepeatGlyph(kHoriz, kColBw + 2) +
         std::string(kCross) + RepeatGlyph(kHoriz, kColLat + 1);
}

std::string FormatBytes(std::uint64_t bytes) {
  const auto value = static_cast<double>(bytes);
  if (bytes >= kBytesPerGiB) {
    return fmt::format("{:.1f} GiB", value / static_cast<double>(kBytesPerGiB));
  }
  if (bytes >= kBytesPerMiB) {
    return fmt::format("{:.1f} MiB", value / static_cast<double>(kBytesPerMiB));
  }
  if (bytes >= kBytesPerKiB) {
    return fmt::format("{:.1f} KiB", value / static_cast<double>(kBytesPerKiB));
  }
  return fmt::format("{} B", bytes);
}

std::string FormatJobConfig(const JobConfig& config) {
  return fmt::format("{} bs={} iodepth={}", JobConfig::ToString(config.rw_mode),
                     SizeParser::Format(config.block_size), config.iodepth);
}

}  // namespace

TerminalDisplay::TerminalDisplay(DisplayContext context)
    : TerminalDisplay(std::move(context), std::cout) {
}

TerminalDisplay::TerminalDisplay(DisplayContext context, std::ostream& out)
    : context_(std::move(context)), out_(&out) {
}

void TerminalDisplay::Init(int runtime_seconds) {
  runtime_seconds_ = runtime_seconds;
  start_ = std::chrono::steady_clock::now();
  *out_ << kClearScreen << kCursorHome << kHideCursor << RenderLiveView(MetricsSnapshot{}, 0);
  out_->flush();
}

void TerminalDisplay::Update(const MetricsSnapshot& snapshot) {
  int elapsed = 0;
  if (start_ != std::chrono::steady_clock::time_point{}) {
    const auto delta =
        std::chrono::duration_cast<std::chrono::seconds>(snapshot.timestamp - start_).count();
    elapsed = static_cast<int>(std::clamp<std::int64_t>(delta, 0, runtime_seconds_));
  }
  *out_ << kCursorHome << RenderLiveView(snapshot, elapsed);
  out_->flush();
}

void TerminalDisplay::ShowSummary(const BenchmarkResults& results) {
  *out_ << kClearScreen << kCursorHome << RenderSummary(results);
  out_->flush();
}

void TerminalDisplay::Shutdown() {
  *out_ << kShowCursor << '\n';
  out_->flush();
}

std::string TerminalDisplay::RenderLiveView(const MetricsSnapshot& snapshot,
                                            int elapsed_seconds) const {
  std::string frame;
  const auto add_line = [&frame](const std::string& content) {
    frame += content;
    frame += kClearLine;
    frame += '\n';
  };

  add_line(fmt::format("C-FIO Benchmark {} Running [{} / {}]    Engine: {}    Direct: {}", kDash,
                       FormatDuration(elapsed_seconds), FormatDuration(runtime_seconds_),
                       context_.engine_label, context_.direct_label));
  add_line(FormatProgressBar(elapsed_seconds, runtime_seconds_, kBarWidth));
  add_line(RepeatGlyph(kDouble, kRowWidth));
  add_line(DataRow("Job", "IOPS (inst/avg)", "BW (inst/avg)", "Lat P50/P95/P99"));
  add_line(SeparatorRow());

  for (const auto& job : snapshot.jobs) {
    add_line(DataRow(job.job_name, FormatIopsCell(job.iops_instant, job.iops_cumulative),
                     FormatBwCell(job.bw_instant, job.bw_cumulative),
                     FormatLatCell(job.lat_p50_ns, job.lat_p95_ns, job.lat_p99_ns)));
  }

  add_line(SeparatorRow());
  const PerJobMetrics& agg = snapshot.aggregate;
  add_line(DataRow("TOTAL", FormatIopsCell(agg.iops_instant, agg.iops_cumulative),
                   FormatBwCell(agg.bw_instant, agg.bw_cumulative), ""));
  add_line(RepeatGlyph(kDouble, kRowWidth));
  add_line(fmt::format(" Errors: R:{} W:{}  {}  Log: {}", agg.read_errors, agg.write_errors, kVert,
                       context_.log_path.string()));

  frame += kClearBelow;
  return frame;
}

std::string TerminalDisplay::RenderSummary(const BenchmarkResults& results) const {
  std::string frame;
  const auto add_line = [&frame](const std::string& content) {
    frame += content;
    frame += '\n';
  };

  add_line(fmt::format("C-FIO Benchmark {} Complete   Runtime {}   Engine {}   Direct {}", kDash,
                       FormatDuration(results.runtime_seconds), context_.engine_label,
                       context_.direct_label));
  add_line(RepeatGlyph(kDouble, kRowWidth));

  std::uint64_t total_iops = 0;
  std::uint64_t total_bw = 0;
  std::uint64_t total_bytes = 0;
  std::uint64_t total_read_errors = 0;
  std::uint64_t total_write_errors = 0;

  bool first = true;
  for (const auto& job : results.jobs) {
    if (!first) {
      add_line(RepeatGlyph(kHoriz, kRowWidth));
    }
    first = false;

    add_line(fmt::format(" {}    {}", job.name, FormatJobConfig(job.config)));
    add_line(fmt::format("   IOPS {}   BW {} MB/s   IO {} ({} ops)", FormatCount(job.iops_avg),
                         job.bw_avg_bytes / kBytesPerMiB, FormatBytes(job.total_bytes),
                         FormatCount(job.total_ios)));
    add_line(
        fmt::format("   Lat {}s  min {} p50 {} p95 {} p99 {} max {}   Err R:{} W:{}", kMicro,
                    FormatCount(job.lat_min_ns / kNsPerUs), FormatCount(job.lat_p50_ns / kNsPerUs),
                    FormatCount(job.lat_p95_ns / kNsPerUs), FormatCount(job.lat_p99_ns / kNsPerUs),
                    FormatCount(job.lat_max_ns / kNsPerUs), job.read_errors, job.write_errors));

    total_iops += job.iops_avg;
    total_bw += job.bw_avg_bytes;
    total_bytes += job.total_bytes;
    total_read_errors += job.read_errors;
    total_write_errors += job.write_errors;
  }

  add_line(RepeatGlyph(kDouble, kRowWidth));
  add_line(fmt::format(" TOTAL   IOPS {}   BW {} MB/s   IO {}   Err R:{} W:{}",
                       FormatCount(total_iops), total_bw / kBytesPerMiB, FormatBytes(total_bytes),
                       total_read_errors, total_write_errors));
  add_line(fmt::format(" Log: {}", context_.log_path.string()));

  return frame;
}

}  // namespace cfio
