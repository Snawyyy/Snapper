#include "ui/mode_icons.h"

#include <QPainter>
#include <QPixmap>
#include <QPolygonF>

#include <cassert>

#include "ui/theme.h"

namespace snapper {
namespace {

void DrawPuppet(QPainter* painter) {
  assert(painter != nullptr);
  assert(painter->isActive());
  painter->drawEllipse(QPointF(8, 3.5), 2.5, 2.5);
  painter->drawLine(QPointF(8, 6), QPointF(8, 11));
  painter->drawLine(QPointF(3, 8), QPointF(13, 8));
  painter->drawLine(QPointF(8, 11), QPointF(4.5, 15));
  painter->drawLine(QPointF(8, 11), QPointF(11.5, 15));
}

void DrawBones(QPainter* painter) {
  assert(painter != nullptr);
  assert(painter->isActive());
  painter->drawLine(QPointF(3.5, 12.5), QPointF(8, 8));
  painter->drawLine(QPointF(8, 8), QPointF(12.5, 3.5));
  painter->drawEllipse(QPointF(3.5, 12.5), 2, 2);
  painter->drawEllipse(QPointF(8, 8), 2, 2);
  painter->drawEllipse(QPointF(12.5, 3.5), 2, 2);
}

void DrawFilm(QPainter* painter) {
  assert(painter != nullptr);
  assert(painter->isActive());
  painter->drawRect(QRectF(1.5, 3.5, 13, 9));
  painter->setBrush(painter->pen().color());
  painter->drawPolygon(
      QPolygonF({QPointF(6.5, 6), QPointF(6.5, 10), QPointF(10, 8)}));
}

}  // namespace

QIcon ModeIcon(int index, bool is_picked) {
  assert(index >= 0);
  assert(index <= 2);
  QPixmap pixmap(kModeIconPx, kModeIconPx);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(is_picked ? theme::kPick : theme::kText, 1.4));
  painter.setBrush(Qt::NoBrush);
  const bool is_pose = index == 0;
  const bool is_rig = index == 1;
  if (is_pose) {
    DrawPuppet(&painter);
  } else if (is_rig) {
    DrawBones(&painter);
  } else {
    DrawFilm(&painter);
  }
  return QIcon(pixmap);
}

}  // namespace snapper
