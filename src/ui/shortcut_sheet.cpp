#include "ui/shortcut_sheet.h"

#include <QHeaderView>
#include <QTableWidgetItem>

#include <cassert>
#include <memory>

#include "ui/panel_kit.h"
#include "ui/screen_fit.h"

namespace snapper {
namespace {

// The sheet's size in ems; FitDialog caps it to the screen.
const QSizeF kSheetEms(40.0, 34.0);

}  // namespace

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
  // The table is its section's body; the sheet owns the section.
  layout_.addWidget(
      PlatedSection(tr("Menu keys, then gestures"), &table_, this));
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
  const bool is_closed = !isVisible();
  if (is_closed) {
    FitDialog(this, kSheetEms);
  }
  show();
  raise();
}

}  // namespace snapper
