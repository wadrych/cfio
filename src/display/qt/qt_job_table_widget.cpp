/// @file qt_job_table_widget.cpp
/// @brief Implementation of the live metrics table widget

#include "display/qt/qt_job_table_widget.h"

#include <cstddef>
#include <string>
#include <vector>

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QHeaderView>
#include <QString>
#include <QStringList>
#include <QTableWidgetItem>
#include <QVariant>
#include <Qt>

#include "display/qt/qt_job_table_model.h"
#include "telemetry/metrics_snapshot.h"

namespace cfio {
namespace {

constexpr int kErrorRed = 0xC0;
constexpr int kErrorGreen = 0x39;
constexpr int kErrorBlue = 0x2B;

constexpr int kJobColumn = static_cast<int>(JobTableColumn::kJob);
constexpr int kErrorsColumn = static_cast<int>(JobTableColumn::kErrors);

}  // namespace

QtJobTableWidget::QtJobTableWidget(QWidget* parent)
    : QTableWidget(0, kJobTableColumnCount, parent) {
  QStringList labels;
  for (const std::string& header : JobTableHeaders()) {
    labels << QString::fromStdString(header);
  }
  setHorizontalHeaderLabels(labels);
  horizontalHeaderItem(kJobColumn)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setSelectionMode(QAbstractItemView::NoSelection);
  setFocusPolicy(Qt::NoFocus);
  setAlternatingRowColors(true);
  setShowGrid(false);
  verticalHeader()->setVisible(false);
  horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  horizontalHeader()->setSectionResizeMode(kJobColumn, QHeaderView::Stretch);
}

void QtJobTableWidget::SetSnapshot(const MetricsSnapshot& snapshot) {
  const std::vector<JobTableRow> rows = BuildJobTableRows(snapshot);
  const auto row_count = static_cast<int>(rows.size());

  if (rowCount() != row_count) {
    setRowCount(row_count);
    for (int row = 0; row < row_count; ++row) {
      for (int column = 0; column < kJobTableColumnCount; ++column) {
        auto* cell = new QTableWidgetItem;
        cell->setTextAlignment(column == kJobColumn ? Qt::AlignLeft | Qt::AlignVCenter
                                                    : Qt::AlignRight | Qt::AlignVCenter);
        setItem(row, column, cell);
      }
    }
  }

  for (int row = 0; row < row_count; ++row) {
    const JobTableRow& source = rows[static_cast<std::size_t>(row)];
    for (int column = 0; column < kJobTableColumnCount; ++column) {
      QTableWidgetItem* cell = item(row, column);
      cell->setText(QString::fromStdString(source.cells[static_cast<std::size_t>(column)]));

      QFont cell_font = cell->font();
      cell_font.setBold(source.is_total);
      cell->setFont(cell_font);

      if (column == kErrorsColumn) {
        if (source.cells[kErrorsColumn] == "0") {
          cell->setData(Qt::ForegroundRole, QVariant());
        } else {
          cell->setForeground(QBrush(QColor(kErrorRed, kErrorGreen, kErrorBlue)));
        }
        cell->setToolTip(QString::fromStdString(source.error_detail));
      }
    }
  }
}

}  // namespace cfio
