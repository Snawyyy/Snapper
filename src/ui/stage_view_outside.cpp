// StageView's pasteboard: what lies past the frame edge, dimmed.

#include <QKeyEvent>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <algorithm>
#include <cassert>

#include "edit/document_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/stage_view.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// How strong things past the frame edge are drawn.
constexpr double kOutsideStrength = 0.4;
// The stage past the frame edge, a little darker, like a viewfinder.
constexpr QColor kOutsideShade{0, 0, 0, 70};
constexpr QColor kFrameEdge{255, 255, 255, 110};
// The arrow toward a pick off the visible stage: how far in from the
// view edge it sits, and its size.
constexpr double kArrowInset = 16.0;
constexpr double kArrowLong = 12.0;

}  // namespace

void StageView::PaintOutside(const StageFrame& frame, QSize shown,
                             QPainter* painter) {
  assert(painter != nullptr);
  assert(frame.scale > 0.0);
  const QRectF inside(frame.corner, QSizeF(shown));
  QPainterPath around;
  around.addRect(QRectF(rect()));
  around.addRect(inside);
  painter->fillPath(around, kOutsideShade);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  const bool is_drawn = managers_.document->IsOutsideShown() &&
                        shot != nullptr && !shot->layers.empty() &&
                        width() > 0 && height() > 0;
  if (!is_drawn) {
    return;
  }
  // The widget in output pixels, the canvas corner at 0,0.
  const QPoint corner = frame.corner.toPoint();
  const QRect area(-corner, size());
  const QImage outside =
      renderer_.RenderArea(project, *shot, frame.local, frame.scale, area);
  painter->save();
  painter->setClipPath(around);
  painter->setOpacity(kOutsideStrength);
  painter->drawImage(QPoint(0, 0), outside);
  painter->restore();
}

void StageView::PaintFrameEdge(const StageFrame& frame, QSize shown,
                               QPainter* painter) const {
  assert(painter != nullptr);
  assert(frame.scale > 0.0);
  painter->save();
  painter->setPen(QPen(kFrameEdge, 1.0));
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(QRectF(frame.corner, QSizeF(shown)).adjusted(-0.5, -0.5,
                                                                 0.5, 0.5));
  painter->restore();
}

bool StageView::OutsideKey(QKeyEvent* event) {
  assert(event != nullptr);
  assert(managers_.document != nullptr);
  const bool is_outside_key = event->key() == Qt::Key_O &&
                              event->modifiers() == Qt::NoModifier;
  if (!is_outside_key) {
    return false;
  }
  managers_.document->SetOutsideShown(!managers_.document->IsOutsideShown());
  feedback_.Pulse(QPointF(width() / 2.0, height() / 2.0),
                  managers_.document->IsOutsideShown()
                      ? tr("Outside shown")
                      : tr("Outside hidden"));
  update();
  return true;
}

void StageView::PaintOffArrows(const StageFrame& frame, QPainter* painter) {
  assert(painter != nullptr);
  assert(managers_.selection != nullptr);
  const QRectF edge = QRectF(rect()).adjusted(kArrowInset, kArrowInset,
                                              -kArrowInset, -kArrowInset);
  const bool has_room = edge.width() > 0.0 && edge.height() > 0.0;
  if (!has_room) {
    return;
  }
  painter->save();
  painter->resetTransform();
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kPick);
  for (const LayerId layer : managers_.selection->PickedLayers()) {
    const QRectF box = BoxOf(frame, {layer});
    const bool is_away = !box.isNull() && !box.intersects(QRectF(rect()));
    if (!is_away) {
      continue;
    }
    const QPointF target = box.center();
    const QPointF spot(std::clamp(target.x(), edge.left(), edge.right()),
                       std::clamp(target.y(), edge.top(), edge.bottom()));
    const QLineF way(spot, target);
    painter->save();
    painter->translate(spot);
    painter->rotate(-way.angle());
    const double half = kArrowLong / 2;
    painter->drawPolygon(QPolygonF(
        {QPointF(half, 0), QPointF(-half, -half), QPointF(-half, half)}));
    painter->restore();
  }
  painter->restore();
}

}  // namespace snapper
