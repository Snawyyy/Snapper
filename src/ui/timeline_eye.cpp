#include "ui/timeline_eye.h"

#include <cassert>

#include "ui/theme.h"
#include "ui/timeline_layout.h"

namespace snapper {

QRectF EyeRect(double top) {
  assert(top >= 0.0);
  assert(kNameWidth > kEyeSize);
  return QRectF(kNameWidth - kEyeSize - 6.0,
                top + (kRowHeight - kEyeSize) / 2.0, kEyeSize, kEyeSize);
}

QRectF EyeGrabRect(double top) {
  assert(top >= 0.0);
  assert(kEyeSize > 0.0);
  return EyeRect(top).adjusted(-4, -4, 4, 4);
}

void PaintEye(const QRectF& box, bool is_open, QPainter* painter) {
  assert(painter != nullptr);
  assert(!box.isEmpty());
  painter->setPen(QPen(is_open ? theme::kText : theme::kTextOff, 1.2));
  painter->setBrush(Qt::NoBrush);
  const QRectF lid = box.adjusted(0.0, 3.0, 0.0, -3.0);
  if (is_open) {
    painter->drawEllipse(lid);
    painter->setBrush(theme::kText);
    painter->drawEllipse(box.center(), 2.0, 2.0);
    return;
  }
  painter->drawLine(QPointF(lid.left(), lid.center().y()),
                    QPointF(lid.right(), lid.center().y()));
}

}  // namespace snapper
