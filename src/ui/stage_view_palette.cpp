#include <QCursor>
#include <QKeyEvent>
#include <QPainter>

#include <cassert>

#include "edit/palette_manager.h"
#include "ui/stage_view.h"
#include "ui/theme.h"

namespace snapper {

void StageView::PaintColorFlash(const QImage& image, QPointF corner,
                                QPainter* painter) const {
  assert(painter != nullptr);
  const PaletteManager* palette = managers_.palette;
  const bool is_hovering = palette != nullptr && palette->hovered().isValid();
  if (!is_hovering || image.isNull()) {
    return;
  }
  const QRgb wanted = palette->hovered().rgb();
  const QImage frame = image.convertToFormat(QImage::Format_ARGB32);
  QImage lit(frame.size(), QImage::Format_ARGB32);
  lit.fill(Qt::transparent);
  // Yellow like a handle, so it reads on any color but the yellow ones.
  const QRgb mark = qRgba(theme::kHandle.red(), theme::kHandle.green(),
                          theme::kHandle.blue(), 210);
  for (int y = 0; y < frame.height(); ++y) {
    const auto* in = reinterpret_cast<const QRgb*>(frame.constScanLine(y));
    auto* out = reinterpret_cast<QRgb*>(lit.scanLine(y));
    for (int x = 0; x < frame.width(); ++x) {
      const bool is_match =
          qAlpha(in[x]) == 255 && (in[x] & 0x00FFFFFFU) == (wanted & 0x00FFFFFFU);
      if (is_match) {
        out[x] = mark;
      }
    }
  }
  painter->save();
  painter->resetTransform();
  painter->drawImage(corner, lit);
  painter->restore();
}

bool StageView::PaletteKey(QKeyEvent* event) {
  assert(event != nullptr);
  PaletteManager* palette = managers_.palette;
  const bool is_pick_key = event->key() == Qt::Key_I &&
                           event->modifiers() == Qt::NoModifier &&
                           palette != nullptr && palette->is_shown();
  if (!is_pick_key) {
    return false;
  }
  event->accept();
  const QPoint at =
      (QPointF(mapFromGlobal(QCursor::pos())) - shown_corner_).toPoint();
  const bool is_on_frame = !shown_.isNull() && shown_.rect().contains(at);
  if (!is_on_frame) {
    emit Problem(tr("Put the cursor over the frame to pick its color."));
    return true;
  }
  QColor color = shown_.pixelColor(at);
  color.setAlpha(255);
  palette->Pick(color);
  return true;
}

}  // namespace snapper
