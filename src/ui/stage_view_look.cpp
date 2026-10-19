#include <QKeyEvent>
#include <QPainter>

#include <cassert>

#include "edit/history_manager.h"
#include "edit/palette_manager.h"
#include "ui/checkerboard.h"
#include "ui/stage_view.h"

namespace snapper {
namespace {

// The checker squares behind a see-through shot, in screen pixels.
constexpr double kStageChecker = 12.0;

}  // namespace

void StageView::PaintBehind(const StageFrame& frame, QSize shown,
                            QPainter* painter) const {
  assert(painter != nullptr);
  assert(managers_.history != nullptr);
  const Shot* shot = FindShot(managers_.history->current(), frame.shot);
  const bool is_clear = shot != nullptr && shot->is_transparent;
  if (!is_clear) {
    return;
  }
  const QRectF area(frame.corner, QSizeF(shown));
  if (is_solid_preview_) {
    const bool has_palette = managers_.palette != nullptr;
    painter->fillRect(area, has_palette ? managers_.palette->current()
                                        : shot->background);
    return;
  }
  PaintCheckerboard(painter, area, kStageChecker);
}

bool StageView::SolidKey(QKeyEvent* event, bool is_down) {
  assert(event != nullptr);
  assert(event->type() == QEvent::KeyPress ||
         event->type() == QEvent::KeyRelease);
  const bool is_v = event->key() == Qt::Key_V;
  // Letting go of Shift first still ends the preview on V's release.
  const bool is_solid_key =
      is_v && (is_down ? event->modifiers() == Qt::ShiftModifier
                       : is_solid_preview_);
  if (!is_solid_key) {
    return false;
  }
  const bool is_change = !event->isAutoRepeat() && is_solid_preview_ != is_down;
  if (is_change) {
    is_solid_preview_ = is_down;
    update();
  }
  return true;
}

}  // namespace snapper
