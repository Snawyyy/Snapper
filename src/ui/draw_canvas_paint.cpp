// DrawCanvas's painting: the dimmed doll, the drawing, its frame,
// pixel lines and the brush outline.

#include <QPainter>
#include <QPainterPath>
#include <QRegion>

#include <cassert>
#include <cmath>
#include <set>

#include "edit/draw_manager.h"
#include "edit/history_manager.h"
#include "render/layer_painter.h"
#include "render/pixel_paint.h"
#include "ui/draw_canvas.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// Screen pixels per drawing pixel from which the pixel lines show.
constexpr double kGridZoom = 8.0;
constexpr double kDollDim = 0.35;
constexpr double kOnion = 0.3;

}  // namespace

void DrawCanvas::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(managers_.draw != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), theme::kFaceDark);
  if (!managers_.draw->is_drawing()) {
    return;
  }
  PaintDoll(&painter);
  PaintDrawing(&painter);
  PaintGrid(&painter);
  PaintCursor(&painter);
}

void DrawCanvas::PaintDoll(QPainter* painter) {
  assert(painter != nullptr);
  assert(managers_.history != nullptr);
  const Project& project = managers_.history->current();
  const Doll* doll = FindDoll(project, managers_.draw->doll());
  if (doll == nullptr) {
    return;
  }
  std::set<QString> others;
  for (const RigPiece& piece : doll->rig.pieces) {
    if (piece.name != managers_.draw->piece()) {
      others.insert(piece.name);
    }
  }
  painter->save();
  painter->setOpacity(kDollDim);
  PaintLayer(project, RestLayer(doll->name), Frame(0), View(), nullptr,
             &cache_, painter, &others);
  painter->restore();
}

void DrawCanvas::PaintDrawing(QPainter* painter) {
  assert(painter != nullptr);
  const DrawManager* draw = managers_.draw;
  assert(draw != nullptr);
  const QRectF frame(QPointF(), QSizeF(draw->image().size()));
  painter->save();
  painter->setTransform(World());
  painter->fillRect(frame, QColor(255, 255, 255, 40));
  const bool is_onion = draw->is_onion() && !draw->is_peeking();
  if (is_onion) {
    painter->setOpacity(kOnion);
    painter->drawImage(QPointF(), draw->reference());
    painter->setOpacity(1.0);
  }
  painter->drawImage(QPointF(), draw->is_peeking() ? draw->reference()
                                                   : draw->image());
  QPen edge(is_outside_ ? theme::kPick : theme::kHandle, 1.0);
  edge.setCosmetic(true);
  painter->setPen(edge);
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(frame);
  painter->restore();
  const QString note = draw->is_peeking() ? tr("Original")
                       : is_outside_ ? tr("Past the frame: cut off")
                                     : QString();
  if (!note.isEmpty()) {
    painter->setPen(is_outside_ ? theme::kPick : theme::kText);
    painter->drawText(rect().adjusted(80, 0, 0, -12),
                      Qt::AlignBottom | Qt::AlignLeft, note);
  }
}

void DrawCanvas::PaintGrid(QPainter* painter) {
  assert(painter != nullptr);
  const QTransform world = World();
  const double pixel = std::sqrt(std::abs(world.determinant()));
  if (pixel < kGridZoom) {
    return;
  }
  const QRect image = managers_.draw->image().rect();
  const QRect seen = world.inverted().mapRect(QRectF(rect())).toAlignedRect();
  const QRect lines = seen.intersected(image);
  assert(lines.width() <= image.width());
  painter->save();
  painter->setTransform(world);
  QPen pen(QColor(128, 128, 128, 90), 1.0);
  pen.setCosmetic(true);
  painter->setPen(pen);
  for (int x = lines.left(); x <= lines.right() + 1; ++x) {
    painter->drawLine(QPointF(x, lines.top()), QPointF(x, lines.bottom() + 1));
  }
  for (int y = lines.top(); y <= lines.bottom() + 1; ++y) {
    painter->drawLine(QPointF(lines.left(), y), QPointF(lines.right() + 1, y));
  }
  painter->restore();
}

void DrawCanvas::PaintCursor(QPainter* painter) {
  assert(painter != nullptr);
  const DrawManager* draw = managers_.draw;
  const bool is_brush = !is_alt_ && (draw->tool() == DrawTool::kBrush ||
                                     draw->tool() == DrawTool::kEraser);
  if (!has_cursor_ || !is_brush) {
    return;
  }
  const QPoint at = PixelAt(cursor_);
  QRegion dab;
  for (const QPoint& offset : DabPixels(draw->brush_size())) {
    dab += QRect(at + offset, QSize(1, 1));
  }
  QPainterPath outline;
  outline.addRegion(dab);
  painter->save();
  painter->setTransform(World());
  QPen dark(QColor(0, 0, 0, 200), 2.0);
  dark.setCosmetic(true);
  painter->strokePath(outline.simplified(), dark);
  QPen light(QColor(255, 255, 255, 230), 1.0);
  light.setCosmetic(true);
  painter->strokePath(outline.simplified(), light);
  painter->restore();
}

}  // namespace snapper
