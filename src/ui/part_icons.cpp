#include "ui/part_icons.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <cassert>

#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kIconSide = 32;

void DrawMark(PartKind kind, QPainter* painter) {
  assert(painter != nullptr);
  assert(static_cast<int>(kind) < kPartKindCount);
  painter->setPen(QPen(theme::kText, 2.4, Qt::SolidLine, Qt::RoundCap,
                       Qt::RoundJoin));
  painter->setBrush(Qt::NoBrush);
  switch (kind) {
    case PartKind::kVector: {
      QPainterPath curve(QPointF(5, 25));
      curve.cubicTo(QPointF(9, 4), QPointF(22, 28), QPointF(27, 7));
      painter->drawPath(curve);
      painter->setBrush(theme::kHandle);
      painter->setPen(QPen(theme::kShadow, 1.0));
      painter->drawRect(QRectF(3, 23, 5, 5));
      painter->drawRect(QRectF(25, 5, 5, 5));
      break;
    }
    case PartKind::kBone: {
      // A bone: a shaft with a round knuckle at each end.
      painter->setPen(QPen(theme::kText, 4.0, Qt::SolidLine, Qt::RoundCap));
      painter->drawLine(QPointF(9, 23), QPointF(23, 9));
      painter->setPen(QPen(theme::kShadow, 1.0));
      painter->setBrush(theme::kText);
      painter->drawEllipse(QPointF(6, 23), 3.6, 3.6);
      painter->drawEllipse(QPointF(9, 26), 3.6, 3.6);
      painter->drawEllipse(QPointF(23, 6), 3.6, 3.6);
      painter->drawEllipse(QPointF(26, 9), 3.6, 3.6);
      break;
    }
    case PartKind::kGroup:
      painter->drawPolyline(QPolygonF{QPointF(4, 9), QPointF(12, 9),
                                      QPointF(15, 12), QPointF(28, 12),
                                      QPointF(28, 26), QPointF(4, 26),
                                      QPointF(4, 9)});
      break;
    case PartKind::kSwitch:
      painter->drawRect(QRectF(9, 4, 17, 13));
      painter->setBrush(theme::kRaised);
      painter->drawRect(QRectF(5, 13, 17, 13));
      painter->setPen(QPen(theme::kPick, 2.4));
      painter->drawLine(QPointF(10, 20), QPointF(17, 20));
      break;
    case PartKind::kImage:
      painter->drawRect(QRectF(4, 6, 24, 20));
      painter->drawPolyline(
          QPolygonF{QPointF(6, 23), QPointF(13, 14), QPointF(18, 20),
                    QPointF(21, 17), QPointF(26, 23)});
      painter->drawEllipse(QPointF(22, 11), 2.0, 2.0);
      break;
  }
}

}  // namespace

QIcon PartIcon(PartKind kind) {
  assert(static_cast<int>(kind) >= 0);
  assert(static_cast<int>(kind) < kPartKindCount);
  QPixmap pixmap(kIconSide, kIconSide);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  DrawMark(kind, &painter);
  return QIcon(pixmap);
}

QString PartKindName(PartKind kind) {
  assert(static_cast<int>(kind) >= 0);
  assert(static_cast<int>(kind) < kPartKindCount);
  switch (kind) {
    case PartKind::kGroup:
      return QCoreApplication::translate("snapper", "Group");
    case PartKind::kBone:
      return QCoreApplication::translate("snapper", "Bones");
    case PartKind::kVector:
      return QCoreApplication::translate("snapper", "Drawing");
    case PartKind::kSwitch:
      return QCoreApplication::translate("snapper", "Switch");
    case PartKind::kImage:
      return QCoreApplication::translate("snapper", "Picture");
  }
  return QString();
}

}  // namespace snapper
