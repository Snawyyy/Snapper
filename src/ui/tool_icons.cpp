#include "ui/tool_icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <cassert>

#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kIconSide = 32;

// A yellow handle square, the mark of a point.
void DrawDot(QPainter* painter, QPointF at) {
  assert(painter != nullptr);
  painter->save();
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kHandle);
  painter->drawRect(QRectF(at.x() - 2.5, at.y() - 2.5, 5, 5));
  painter->restore();
}

void DrawArrow(QPainter* painter) {
  assert(painter != nullptr);
  painter->setBrush(theme::kText);
  painter->drawPolygon(QPolygonF{QPointF(9, 4), QPointF(9, 25),
                                 QPointF(14, 20), QPointF(18, 28),
                                 QPointF(21, 27), QPointF(17, 19),
                                 QPointF(24, 19)});
}

void DrawCross(QPainter* painter) {
  assert(painter != nullptr);
  painter->drawLine(QPointF(16, 4), QPointF(16, 28));
  painter->drawLine(QPointF(4, 16), QPointF(28, 16));
  painter->setBrush(theme::kText);
  painter->drawPolygon(QPolygonF{QPointF(16, 2), QPointF(12, 7),
                                 QPointF(20, 7)});
  painter->drawPolygon(QPolygonF{QPointF(16, 30), QPointF(12, 25),
                                 QPointF(20, 25)});
  painter->drawPolygon(QPolygonF{QPointF(2, 16), QPointF(7, 12),
                                 QPointF(7, 20)});
  painter->drawPolygon(QPolygonF{QPointF(30, 16), QPointF(25, 12),
                                 QPointF(25, 20)});
}

void DrawNib(QPainter* painter) {
  assert(painter != nullptr);
  QPainterPath curve(QPointF(4, 28));
  curve.cubicTo(QPointF(8, 18), QPointF(14, 24), QPointF(17, 15));
  painter->drawPath(curve);
  painter->drawPolygon(QPolygonF{QPointF(17, 15), QPointF(21, 6),
                                 QPointF(28, 4), QPointF(26, 11)});
  DrawDot(painter, QPointF(4, 28));
}

void DrawSquiggle(QPainter* painter) {
  assert(painter != nullptr);
  QPainterPath curve(QPointF(4, 22));
  curve.cubicTo(QPointF(6, 6), QPointF(14, 6), QPointF(15, 16));
  curve.cubicTo(QPointF(16, 26), QPointF(26, 26), QPointF(28, 9));
  painter->drawPath(curve);
}

void DrawCurvature(QPainter* painter) {
  assert(painter != nullptr);
  painter->drawPolyline(QPolygonF{QPointF(4, 26), QPointF(11, 8),
                                  QPointF(18, 26)});
  QPainterPath round(QPointF(16, 26));
  round.cubicTo(QPointF(20, 4), QPointF(26, 4), QPointF(29, 26));
  painter->drawPath(round);
  DrawDot(painter, QPointF(11, 8));
}

void DrawLineWidth(QPainter* painter) {
  assert(painter != nullptr);
  painter->setPen(Qt::NoPen);
  painter->setBrush(theme::kText);
  painter->drawPolygon(QPolygonF{QPointF(4, 21), QPointF(28, 13),
                                 QPointF(28, 25), QPointF(4, 23)});
  painter->setPen(QPen(theme::kText, 2.0, Qt::SolidLine, Qt::RoundCap));
  painter->drawLine(QPointF(10, 4), QPointF(10, 12));
  painter->drawLine(QPointF(7, 7), QPointF(10, 4));
  painter->drawLine(QPointF(13, 7), QPointF(10, 4));
}

void DrawDeleteEdge(QPainter* painter) {
  assert(painter != nullptr);
  painter->drawLine(QPointF(4, 24), QPointF(11, 20));
  painter->drawLine(QPointF(21, 14), QPointF(28, 10));
  DrawDot(painter, QPointF(4, 24));
  DrawDot(painter, QPointF(28, 10));
  painter->setPen(QPen(theme::kPick, 2.4, Qt::SolidLine, Qt::RoundCap));
  painter->drawLine(QPointF(12, 10), QPointF(20, 24));
  painter->drawLine(QPointF(20, 10), QPointF(12, 24));
}

void DrawMagnet(QPainter* painter) {
  assert(painter != nullptr);
  painter->setPen(QPen(theme::kPick, 5.0, Qt::SolidLine, Qt::FlatCap));
  QPainterPath shoe(QPointF(9, 6));
  shoe.lineTo(QPointF(9, 16));
  shoe.arcTo(QRectF(9, 9, 14, 14), 180, 180);
  shoe.lineTo(QPointF(23, 6));
  painter->drawPath(shoe);
  painter->setPen(QPen(theme::kText, 5.0, Qt::SolidLine, Qt::FlatCap));
  painter->drawLine(QPointF(9, 4), QPointF(9, 9));
  painter->drawLine(QPointF(23, 4), QPointF(23, 9));
}

void DrawBucket(QPainter* painter) {
  assert(painter != nullptr);
  painter->drawPolygon(QPolygonF{QPointF(6, 13), QPointF(16, 5),
                                 QPointF(25, 14), QPointF(15, 24)});
  painter->setPen(Qt::NoPen);
  painter->setBrush(theme::kPick);
  QPainterPath drop(QPointF(26, 18));
  drop.cubicTo(QPointF(22, 24), QPointF(24, 28), QPointF(26, 28));
  drop.cubicTo(QPointF(28, 28), QPointF(30, 24), QPointF(26, 18));
  painter->drawPath(drop);
}

void DrawBone(QPainter* painter) {
  assert(painter != nullptr);
  painter->setPen(QPen(theme::kText, 4.0, Qt::SolidLine, Qt::RoundCap));
  painter->drawLine(QPointF(10, 22), QPointF(22, 10));
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kText);
  painter->drawEllipse(QPointF(7, 22), 3.4, 3.4);
  painter->drawEllipse(QPointF(10, 25), 3.4, 3.4);
  painter->drawEllipse(QPointF(22, 7), 3.4, 3.4);
  painter->drawEllipse(QPointF(25, 10), 3.4, 3.4);
}

void DrawReach(QPainter* painter) {
  assert(painter != nullptr);
  painter->setPen(QPen(theme::kHandle, 1.6, Qt::DashLine));
  painter->drawEllipse(QPointF(16, 16), 13, 9);
  DrawBone(painter);
}

void DrawBind(QPainter* painter) {
  assert(painter != nullptr);
  painter->setPen(QPen(theme::kText, 4.0, Qt::SolidLine, Qt::RoundCap));
  painter->drawLine(QPointF(6, 26), QPointF(18, 14));
  painter->setPen(QPen(theme::kHandle, 1.4, Qt::DotLine));
  painter->drawLine(QPointF(18, 14), QPointF(27, 7));
  painter->drawLine(QPointF(18, 14), QPointF(27, 21));
  DrawDot(painter, QPointF(27, 7));
  DrawDot(painter, QPointF(27, 21));
}

void DrawTryBones(QPainter* painter) {
  assert(painter != nullptr);
  painter->setPen(QPen(theme::kText, 4.0, Qt::SolidLine, Qt::RoundCap));
  painter->drawPolyline(QPolygonF{QPointF(5, 27), QPointF(13, 15),
                                  QPointF(26, 11)});
  painter->setPen(QPen(theme::kHandle, 1.6, Qt::SolidLine, Qt::RoundCap));
  QPainterPath swing(QPointF(26, 19));
  swing.cubicTo(QPointF(29, 16), QPointF(29, 7), QPointF(24, 4));
  painter->drawPath(swing);
}

void DrawMark(CharacterTool tool, QPainter* painter) {
  assert(painter != nullptr);
  assert(static_cast<int>(tool) < kCharacterToolCount);
  painter->setPen(QPen(theme::kText, 2.2, Qt::SolidLine, Qt::RoundCap,
                       Qt::RoundJoin));
  painter->setBrush(Qt::NoBrush);
  switch (tool) {
    case CharacterTool::kPick:
      DrawArrow(painter);
      break;
    case CharacterTool::kMovePart:
      DrawCross(painter);
      break;
    case CharacterTool::kAddPoint:
      DrawNib(painter);
      break;
    case CharacterTool::kFreehand:
      DrawSquiggle(painter);
      break;
    case CharacterTool::kOval:
      painter->drawEllipse(QPointF(16, 16), 12, 9);
      break;
    case CharacterTool::kRectangle:
      painter->drawRect(QRectF(5, 8, 22, 16));
      break;
    case CharacterTool::kCurvature:
      DrawCurvature(painter);
      break;
    case CharacterTool::kLineWidth:
      DrawLineWidth(painter);
      break;
    case CharacterTool::kDeleteEdge:
      DrawDeleteEdge(painter);
      break;
    case CharacterTool::kMagnet:
      DrawMagnet(painter);
      break;
    case CharacterTool::kFill:
      DrawBucket(painter);
      break;
    case CharacterTool::kStrength:
      DrawReach(painter);
      break;
    case CharacterTool::kBindPoints:
      DrawBind(painter);
      break;
    case CharacterTool::kTryBones:
      DrawTryBones(painter);
      break;
  }
}

}  // namespace

QIcon ToolIcon(CharacterTool tool) {
  assert(static_cast<int>(tool) >= 0);
  assert(static_cast<int>(tool) < kCharacterToolCount);
  QPixmap pixmap(kIconSide, kIconSide);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  DrawMark(tool, &painter);
  return QIcon(pixmap);
}

}  // namespace snapper
