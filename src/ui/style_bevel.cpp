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
  // Modern retro: a flat face with a light line along its top and a dark
  // one under it when raised, the dark line on top when sunk; the sides
  // are a quiet edge a shade under the face.
  painter->fillRect(rect, fill);
  painter->save();
  painter->setPen(fill.darker(135));
  painter->drawLine(rect.topLeft(), rect.bottomLeft());
  painter->drawLine(rect.topRight(), rect.bottomRight());
  painter->setPen(is_sunk ? theme::kShadow : fill.lighter(128));
  painter->drawLine(rect.topLeft(), rect.topRight());
  painter->setPen(is_sunk ? fill.darker(135) : theme::kShadow);
  painter->drawLine(rect.bottomLeft(), rect.bottomRight());
  painter->restore();
}

void DrawSunkenFrame(QPainter* painter, const QRect& rect) {
  assert(painter != nullptr);
  assert(rect.isValid());
  painter->save();
  painter->setPen(theme::kFaceDark);
  painter->drawRect(rect.adjusted(0, 0, -1, -1));
  painter->setPen(theme::kShadow);
  painter->drawLine(rect.topLeft(), rect.topRight());
  painter->restore();
}

}  // namespace snapper
