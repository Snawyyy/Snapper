#include "ui/shortcut_sheet.h"

#include <QHeaderView>
#include <QTableWidgetItem>

#include <cassert>
#include <memory>

namespace snapper {

ShortcutSheet::ShortcutSheet(QMenuBar* bar, QWidget* parent)
    : QDialog(parent), bar_(bar), layout_(this) {
  assert(bar_ != nullptr);
  assert(parent != nullptr);
  setObjectName("shortcut_sheet");
  setWindowTitle(tr("Keys and gestures"));
  table_.setObjectName("shortcut_table");
  table_.setColumnCount(2);
  table_.setHorizontalHeaderLabels({tr("Keys"), tr("What it does")});
  table_.horizontalHeader()->setStretchLastSection(true);
  table_.verticalHeader()->hide();
  table_.setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_.setSelectionMode(QAbstractItemView::NoSelection);
  layout_.addWidget(&table_);
  resize(560, 480);
}

void ShortcutSheet::Open() {
  assert(bar_ != nullptr);
  assert(table_.columnCount() == 2);
  table_.setRowCount(0);
  const auto add = [this](const QString& keys, const QString& does) {
    const int row = table_.rowCount();
    table_.insertRow(row);
    // The table owns its items from here.
    table_.setItem(row, 0,
                   std::make_unique<QTableWidgetItem>(keys).release());
    table_.setItem(row, 1,
                   std::make_unique<QTableWidgetItem>(does).release());
  };
  for (const Command& command : CommandsOf(bar_)) {
    const QString keys = command.action->shortcut().toString();
    const bool has_keys = !keys.isEmpty();
    if (has_keys) {
      add(keys, command.name);
    }
  }
  for (const Gesture& gesture : kGestures) {
    add(tr(gesture.keys), tr(gesture.does));
  }
  table_.resizeColumnToContents(0);
  show();
  raise();
}

}  // namespace snapper
