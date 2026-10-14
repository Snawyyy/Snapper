#include "render/storyboard_paint.h"

#include <QPainter>

#include <cassert>

namespace snapper {

void PaintStoryboard(const StoryboardLayer& board, Frame local,
                     ImageCache* cache, QImage* frame) {
  assert(cache != nullptr && frame != nullptr);
  assert(board.opacity >= 0.0 && board.opacity <= 1.0);
  const bool is_seen = board.opacity > 0.0;
  const StoryboardCel* cel = is_seen ? CelAt(board, local) : nullptr;
  const bool has_cel = cel != nullptr;
  if (!has_cel) {
    return;
  }
  const QImage& picture = cache->Get(cel->path);
  const bool has_picture = !picture.isNull();
  if (!has_picture) {
    return;
  }
  QPainter painter(frame);
  painter.setOpacity(board.opacity);
  painter.setRenderHint(QPainter::SmoothPixmapTransform,
                        frame->size() != picture.size());
  painter.drawImage(QRectF(QPointF(), QSizeF(frame->size())), picture);
}

QImage TintedCel(const QImage& image, QColor color) {
  assert(color.isValid());
  const bool is_empty = image.isNull();
  if (is_empty) {
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
