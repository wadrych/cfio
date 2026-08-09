/// @file qt_display.cpp
/// @brief Implementation of the mailbox backed display backend

#include "display/qt/qt_display.h"

#include <utility>

#include "display/display_context.h"
#include "telemetry/benchmark_results.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {

QtDisplay::QtDisplay(RunMailbox& mailbox, DisplayContext context)
    : mailbox_(&mailbox), context_(std::move(context)) {
}

void QtDisplay::Init(int runtime_seconds) {
  mailbox_->SetRuntime(runtime_seconds);
}

void QtDisplay::Update(const MetricsSnapshot& snapshot) {
  mailbox_->PublishSnapshot(snapshot);
}

void QtDisplay::ShowSummary(const BenchmarkResults& results) {
  mailbox_->PublishResults(results);
}

void QtDisplay::Shutdown() {
  mailbox_->MarkShutdown();
}

bool QtDisplay::StopRequested() const {
  return mailbox_->StopRequested();
}

const DisplayContext& QtDisplay::Context() const {
  return context_;
}

}  // namespace cfio
