// PalettePanel docked over the inspector while drawing.

#include <QEvent>

#include <cassert>

#include "ui/palette_panel.h"

namespace snapper {

void PalettePanel::DockOver(QWidget* area) {
  assert(area != nullptr);
  assert(parentWidget() != nullptr);
  const bool is_same = dock_area_ == area;
  if (is_same) {
    return;
  }
  Undock();
  // Keeps where it floated, to go back there after drawing.
  KeepPlace();
  dock_area_ = area;
  area->installEventFilter(this);
  setCursor(Qt::ArrowCursor);
  Follow();
}

void PalettePanel::Undock() {
  assert(palette_ != nullptr);
  assert(width() >= 0);
  const bool has_area = !dock_area_.isNull();
  if (!has_area) {
    return;
  }
  dock_area_->removeEventFilter(this);
  dock_area_.clear();
  setCursor(Qt::SizeAllCursor);
  const bool is_shown = isVisible();
  if (is_shown) {
    RestorePlace();
    FitHeight();
  }
}

void PalettePanel::Follow() {
  assert(is_docked());
  assert(parentWidget() != nullptr);
  const QWidget* area = dock_area_;
  setGeometry(QRect(area->mapTo(parentWidget(), QPoint(0, 0)),
                    area->size()));
  raise();
}

bool PalettePanel::eventFilter(QObject* watched, QEvent* event) {
  assert(watched != nullptr);
  assert(event != nullptr);
  const QEvent::Type type = event->type();
  const bool moves_area = is_docked() && watched == dock_area_ &&
                          (type == QEvent::Resize || type == QEvent::Move ||
                           type == QEvent::Show);
  if (moves_area) {
    Follow();
  }
  // The area went away (full view hid it): float where it was left.
  const bool loses_area = watched == dock_area_ && type == QEvent::Hide &&
                          isVisible();
  if (loses_area) {
    RestorePlace();
    raise();
  }
  return QWidget::eventFilter(watched, event);
}

}  // namespace snapper
