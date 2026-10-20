#include "ui/bevel.h"

#include <QPainter>

#include <cassert>

#include "ui/theme.h"

namespace snapper {
namespace {

// Thicker than this stops reading as an edge.
constexpr int kMaxBevel = 4;

}  // namespace

void DrawBevel(QPainter* painter, const QRectF& box, bool is_sunk,
               int width) {
  assert(painter != nullptr);
  assert(width > 0 && width <= kMaxBevel);
  // The style's edge (style_bevel.h): the light or dark line runs along
  // the top and bottom only; the sides stay quiet.
  const QColor top = is_sunk ? theme::kShadow : theme::kFaceLight;
  const QColor bottom = is_sunk ? theme::kFaceDark : theme::kShadow;
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, false);
  for (int ring = 0; ring < width; ++ring) {
    const QRectF edge = box.adjusted(ring, ring, -ring, -ring);
    painter->setPen(theme::kFaceDark);
    painter->drawLine(edge.topLeft(), edge.bottomLeft());
    painter->drawLine(edge.topRight(), edge.bottomRight());
    painter->setPen(top);
    painter->drawLine(edge.topLeft(), edge.topRight());
    painter->setPen(bottom);
    painter->drawLine(edge.bottomLeft(), edge.bottomRight());
  }
  painter->restore();
}

}  // namespace snapper
