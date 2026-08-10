/// @file qt_main_window.cpp
/// @brief Implementation of the timer driven main GUI window

#include "display/qt/qt_main_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "display/display_context.h"
#include "display/metric_format.h"
#include "display/qt/qt_chart_geometry.h"
#include "display/qt/qt_job_table_model.h"
#include "display/qt/run_mailbox.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

constexpr int kPollIntervalMs = 200;
constexpr int kMsPerSecond = 1000;
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 720;
constexpr int kTableStretch = 2;
constexpr int kGraphStretch = 3;
constexpr int kProgressMinWidth = 240;

constexpr std::array<ChartKind, kGraphCount> kGraphKinds = {ChartKind::kIops, ChartKind::kBandwidth,
                                                            ChartKind::kLatency};

/// @brief Engine and direct part of the header.
/// @param context  Run metadata.
/// @return Text for the left header label.
QString HeaderRunText(const DisplayContext& context) {
  return QString::fromStdString("Engine: " + context.engine_label +
                                "    Direct: " + context.direct_label);
}

/// @brief Log path part of the header.
/// @param context  Run metadata.
/// @return Text for the right header label, a placeholder until the run starts.
QString HeaderLogText(const DisplayContext& context) {
  if (context.log_path.empty()) {
    return QStringLiteral("Log: pending");
  }
  return QString::fromStdString("Log: " + context.log_path.string());
}

}  // namespace

QtMainWindow::QtMainWindow(RunMailbox& mailbox, DisplayContext context, QWidget* parent)
    : QMainWindow(parent),
      mailbox_(&mailbox),
      context_(std::move(context)),
      timer_(new QTimer(this)),
      run_label_(new QLabel(this)),
      log_label_(new QLabel(this)),
      progress_(new QProgressBar(this)),
      status_(new QLabel(this)),
      stop_button_(new QPushButton(QStringLiteral("Stop"), this)),
      table_(new QtJobTableWidget(this)),
      graphs_{new QtGraphWidget(kGraphKinds[0], this), new QtGraphWidget(kGraphKinds[1], this),
              new QtGraphWidget(kGraphKinds[2], this)} {
  setWindowTitle(QStringLiteral("C-FIO"));
  resize(kWindowWidth, kWindowHeight);

  BuildLayout();

  connect(timer_, &QTimer::timeout, this, &QtMainWindow::OnTick);
  connect(stop_button_, &QPushButton::clicked, this, &QtMainWindow::OnStopClicked);

  UpdateProgress();
  timer_->start(kPollIntervalMs);
}

void QtMainWindow::BuildLayout() {
  auto* central = new QWidget(this);
  auto* layout = new QVBoxLayout(central);

  auto* header = new QHBoxLayout;
  run_label_->setText(HeaderRunText(context_));
  log_label_->setText(HeaderLogText(context_));
  log_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  header->addWidget(run_label_);
  header->addStretch();
  header->addWidget(log_label_);
  layout->addLayout(header);

  auto* controls = new QHBoxLayout;
  progress_->setMinimumWidth(kProgressMinWidth);
  progress_->setFormat(QStringLiteral("%p%"));
  stop_button_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  controls->addWidget(progress_, 1);
  controls->addWidget(status_);
  controls->addStretch();
  controls->addWidget(stop_button_);
  layout->addLayout(controls);

  layout->addWidget(table_, kTableStretch);
  for (QtGraphWidget* graph : graphs_) {
    layout->addWidget(graph, kGraphStretch);
  }

  setCentralWidget(central);
}

void QtMainWindow::closeEvent(QCloseEvent* event) {
  mailbox_->RequestStop();
  event->accept();
}

void QtMainWindow::OnTick() {
  const Phase phase = mailbox_->CurrentPhase();
  if (phase != Phase::kIdle && !clock_.isValid()) {
    clock_.start();
  }

  if (std::optional<DisplayContext> context = mailbox_->TakeContext(); context.has_value()) {
    ApplyContext(context.value());
  }
  UpdateProgress();

  const std::uint64_t sequence = mailbox_->Sequence();
  if (sequence != last_sequence_) {
    last_sequence_ = sequence;
    ApplySnapshot(mailbox_->LatestSnapshot());
  }

  if (phase != Phase::kFinished) {
    return;
  }

  if (std::optional<BenchmarkResults> results = mailbox_->TakeResults(); results.has_value()) {
    ApplyResults(results.value());
  } else {
    status_->setText(QStringLiteral("run ended without results, see log"));
  }
  MarkFinished();
}

void QtMainWindow::OnStopClicked() {
  if (finished_) {
    close();
    return;
  }
  mailbox_->RequestStop();
  stop_button_->setEnabled(false);
  stop_button_->setText(QStringLiteral("Stopping..."));
}

void QtMainWindow::ApplyContext(const DisplayContext& context) {
  context_ = context;
  run_label_->setText(HeaderRunText(context_));
  log_label_->setText(HeaderLogText(context_));
}

void QtMainWindow::ApplySnapshot(const MetricsSnapshot& snapshot) {
  history_.push_back(snapshot);
  table_->SetSnapshot(snapshot);
  for (QtGraphWidget* graph : graphs_) {
    graph->SetSeries(history_);
  }
}

void QtMainWindow::ApplyResults(const BenchmarkResults& results) {
  if (!results.time_series.empty()) {
    history_ = results.time_series;
    for (QtGraphWidget* graph : graphs_) {
      graph->SetSeries(history_);
    }
  }
  if (!history_.empty()) {
    table_->SetSnapshot(history_.back());
  }
  status_->setText(QString::fromStdString(BuildRunSummary(results)));
}

void QtMainWindow::MarkFinished() {
  finished_ = true;
  timer_->stop();
  progress_->hide();
  stop_button_->setEnabled(true);
  stop_button_->setText(QStringLiteral("Close"));
}

void QtMainWindow::UpdateProgress() {
  if (mailbox_->CurrentPhase() == Phase::kIdle) {
    progress_->setValue(0);
    status_->setText(QStringLiteral("preparing files..."));
    return;
  }

  const int runtime = mailbox_->RuntimeSeconds();
  const int total_ms = runtime * kMsPerSecond;
  progress_->setMaximum(std::max(total_ms, 1));

  const auto elapsed_ms = clock_.isValid() ? clock_.elapsed() : 0;
  const int value = std::clamp(static_cast<int>(elapsed_ms), 0, total_ms);
  progress_->setValue(value);
  status_->setText(QString::fromStdString(FormatDuration(value / kMsPerSecond) + " / " +
                                          FormatDuration(runtime)));
}

}  // namespace cfio
