#ifndef SNAPPER_UI_PAINT_CLIP_H_
#define SNAPPER_UI_PAINT_CLIP_H_

#include <QPainter>
#include <QRectF>

#include <cassert>

namespace snapper {

// Paints only inside area while it lives, then puts the old clip back.
class PaintClip final {
 public:
  PaintClip(QPainter* painter, const QRectF& area) : painter_(painter) {
    assert(painter_ != nullptr);
    assert(!area.isEmpty());
    painter_->save();
    painter_->setClipRect(area, Qt::IntersectClip);
  }
  ~PaintClip() { painter_->restore(); }

 private:
  QPainter* painter_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_PAINT_CLIP_H_
