#include "ui/style_bevel.h"

#include <cassert>

#include "ui/theme.h"

namespace snapper {

void DrawBevel(QPainter* painter, const QRect& rect, const QColor& fill,
               bool is_sunk) {
  assert(painter != nullptr);
  assert(fill.isValid());
  // A squeezed widget (a combo box in a narrow panel) leaves no room.
  const bool has_room = rect.isValid();
  if (!has_room) {
    return;
  }
  painter->fillRect(rect, fill);
  const QColor top_left = is_sunk ? theme::kShadow : theme::kFaceLight;
  const QColor bottom_right = is_sunk ? theme::kFaceLight : theme::kShadow;
  painter->setPen(top_left);
  painter->drawLine(rect.topLeft(), rect.topRight());
  painter->drawLine(rect.topLeft(), rect.bottomLeft());
  painter->setPen(bottom_right);
  painter->drawLine(rect.bottomLeft(), rect.bottomRight());
  painter->drawLine(rect.topRight(), rect.bottomRight());
  const bool is_raised = !is_sunk;
  if (is_raised) {
    const QRect inner = rect.adjusted(1, 1, -1, -1);
    painter->setPen(theme::kFaceDark);
    painter->drawLine(inner.bottomLeft(), inner.bottomRight());
    painter->drawLine(inner.topRight(), inner.bottomRight());
  }
}

void DrawSunkenFrame(QPainter* painter, const QRect& rect) {
  assert(painter != nullptr);
  assert(rect.isValid());
  painter->save();
  painter->setPen(theme::kShadow);
  painter->drawLine(rect.topLeft(), rect.topRight());
  painter->drawLine(rect.topLeft(), rect.bottomLeft());
  painter->setPen(theme::kFaceLight);
  painter->drawLine(rect.bottomLeft(), rect.bottomRight());
  painter->drawLine(rect.topRight(), rect.bottomRight());
  painter->restore();
}

}  // namespace snapper
