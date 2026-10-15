#include "ui/crt_effect.h"

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPoint>
#include <QTransform>

#include <cassert>

namespace snapper {
namespace {

constexpr int kTileWidth = 3;
constexpr int kTileHeight = 2;
// How much of the other two colors a phosphor lets through: none would
// be a pure stripe, too dark to read; this keeps the glow.
constexpr int kBleed = 110;
// The light lost to the phosphors, given back by adding the masked
// picture onto itself at this strength.
constexpr double kGain = 0.55;

QColor Phosphor(int channel) {
  assert(channel >= 0 && channel < 3);
  assert(kBleed >= 0 && kBleed <= 255);
  return QColor(channel == 0 ? 255 : kBleed, channel == 1 ? 255 : kBleed,
                channel == 2 ? 255 : kBleed);
}

}  // namespace

CrtEffect::CrtEffect() {
  QImage tile(kTileWidth, kTileHeight, QImage::Format_RGB32);
  for (int y = 0; y < kTileHeight; ++y) {
    for (int x = 0; x < kTileWidth; ++x) {
      // Row one r g b, row two shifted one along: b r g.
      tile.setPixelColor(x, y, Phosphor((x + 2 * y) % 3));
    }
  }
  tile_ = QPixmap::fromImage(tile);
  assert(!tile_.isNull());
  assert(tile_.width() == kTileWidth);
}

void CrtEffect::draw(QPainter* painter) {
  assert(painter != nullptr);
  assert(!tile_.isNull());
  QPoint offset;
  const QPixmap source = sourcePixmap(Qt::DeviceCoordinates, &offset);
  const bool has_source = !source.isNull();
  if (!has_source) {
    return;
  }
  // Work in screen pixels, one phosphor each.
  QImage screen = source.toImage().convertToFormat(
      QImage::Format_ARGB32_Premultiplied);
  screen.setDevicePixelRatio(1.0);
  QImage masked = screen;
  {
    QPainter mask(&masked);
    mask.setCompositionMode(QPainter::CompositionMode_Multiply);
    mask.setBrushOrigin(-offset);
    mask.fillRect(masked.rect(), QBrush(tile_));
  }
  QImage lit = masked;
  {
    QPainter glow(&lit);
    glow.setCompositionMode(QPainter::CompositionMode_Plus);
    glow.setOpacity(kGain);
    glow.drawImage(0, 0, masked);
    // Multiply fills see-through pixels too: keep only the widget's own.
    glow.setOpacity(1.0);
    glow.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    glow.drawImage(0, 0, screen);
  }
  painter->save();
  painter->setWorldTransform(QTransform());
  painter->drawImage(offset, lit);
  painter->restore();
}

}  // namespace snapper
