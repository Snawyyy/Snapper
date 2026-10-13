#include "render/draft_paint.h"

#include <QPainter>

#include <cassert>

namespace snapper {

void PaintDraft(const Draft& draft, Frame local, ImageCache* cache,
                QImage* frame) {
  assert(cache != nullptr && frame != nullptr);
  assert(draft.opacity >= 0.0 && draft.opacity <= 1.0);
  const bool is_seen = draft.is_shown && draft.opacity > 0.0;
  const DraftCel* cel = is_seen ? CelAt(draft, local) : nullptr;
  if (cel == nullptr) {
    return;
  }
  const QImage& picture = cache->Get(cel->path);
  if (picture.isNull()) {
    return;
  }
  QPainter painter(frame);
  painter.setOpacity(draft.opacity);
  painter.setRenderHint(QPainter::SmoothPixmapTransform,
                        frame->size() != picture.size());
  painter.drawImage(QRectF(QPointF(), QSizeF(frame->size())), picture);
}

QImage TintedCel(const QImage& image, QColor color) {
  assert(color.isValid());
  if (image.isNull()) {
    return QImage();
  }
  QImage tinted = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  QPainter painter(&tinted);
  painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
  painter.fillRect(tinted.rect(), color);
  painter.end();
  assert(tinted.size() == image.size());
  return tinted;
}

}  // namespace snapper
