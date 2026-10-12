#include "ui/checkerboard.h"

#include <QBrush>
#include <QPixmap>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "ui/theme.h"

namespace snapper {
namespace {

// How much lighter than the stage face the two squares are, in percent.
constexpr int kDarkSquare = 260;
constexpr int kLightSquare = 330;
constexpr double kBadgeSide = 9.0;
constexpr double kBadgeInset = 2.0;

}  // namespace

void PaintCheckerboard(QPainter* painter, const QRectF& area, double cell) {
  assert(painter != nullptr);
  assert(cell > 0.0);
  const int side = std::max(1, static_cast<int>(std::lround(cell)));
  QPixmap tile(side * 2, side * 2);
  tile.fill(theme::kFace.lighter(kDarkSquare));
  {
    QPainter tile_painter(&tile);
    const QColor light = theme::kFace.lighter(kLightSquare);
    tile_painter.fillRect(0, 0, side, side, light);
    tile_painter.fillRect(side, side, side, side, light);
  }
  QBrush brush(tile);
  // The squares start at the area's corner, so they move with it.
  brush.setTransform(QTransform::fromTranslate(area.left(), area.top()));
  painter->fillRect(area, brush);
}

void PaintClearBadge(QPainter* painter, const QRectF& picture) {
  assert(painter != nullptr);
  const QRectF badge(picture.right() - kBadgeSide - kBadgeInset,
                     picture.top() + kBadgeInset, kBadgeSide, kBadgeSide);
  painter->save();
  PaintCheckerboard(painter, badge, kBadgeSide / 3.0);
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(badge);
  painter->restore();
}

}  // namespace snapper
