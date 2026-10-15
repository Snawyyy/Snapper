#include "ui/look_preview.h"

#include <QPainter>
#include <QTransform>

#include <array>
#include <cassert>

#include "ui/deck_paint.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// A round little face, one character a pixel: o outline, f fill,
// h highlight, '.' see-through.
constexpr int kSpriteSize = 10;
constexpr std::array<const char*, kSpriteSize> kSprite = {
    "...oooo...", "..offffo..", ".offhhffo.", "offhhffffo",
    "offfffffff", "offfffffo.", "offfffffo.", ".offfffo..",
    "..offfo...", "...ooo....",
};
constexpr double kTurn = 24.0;
// Alpha below this is cut away with hard edges.
constexpr int kCut = 128;

}  // namespace

LookPreview::LookPreview()
    : sprite_(kSpriteSize, kSpriteSize, QImage::Format_ARGB32_Premultiplied) {
  assert(!sprite_.isNull());
  sprite_.fill(Qt::transparent);
  for (int y = 0; y < kSpriteSize; ++y) {
    for (int x = 0; x < kSpriteSize; ++x) {
      const char at = kSprite[static_cast<size_t>(y)][x];
      const bool is_ink = at != '.';
      if (is_ink) {
        sprite_.setPixelColor(x, y, at == 'o'   ? QColor(0x2a, 0x10, 0x18)
                                    : at == 'h' ? QColor(0xff, 0xd0, 0xd8)
                                                : theme::kPick);
      }
    }
  }
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  assert(sprite_.width() == kSpriteSize);
}

void LookPreview::Show(DrawingLook look, bool has_hard_edges) {
  assert(static_cast<int>(look) >= 0);
  assert(static_cast<int>(look) < kDrawingLookCount);
  look_ = look;
  has_hard_edges_ = has_hard_edges;
  update();
}

QSize LookPreview::sizeHint() const { return QSize(96, 96); }

void LookPreview::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(!sprite_.isNull());
  QPainter painter(this);
  const QRectF glass = QRectF(rect()).adjusted(1, 1, -1, -1);
  deck::PaintDisplay(&painter, glass.toRect());
  // Draw the turned, blown up sprite into its own picture first, so the
  // flat fill and the hard edge cut can work on it.
  QImage shown(size(), QImage::Format_ARGB32_Premultiplied);
  shown.fill(Qt::transparent);
  {
    QPainter turn(&shown);
    const bool is_smooth = look_ != DrawingLook::kCrisp;
    turn.setRenderHint(QPainter::SmoothPixmapTransform, is_smooth);
    const double scale = glass.width() * 0.62 / kSpriteSize;
    QTransform place;
    place.translate(width() / 2.0, height() / 2.0);
    place.rotate(kTurn);
    place.scale(scale, scale);
    place.translate(-kSpriteSize / 2.0, -kSpriteSize / 2.0);
    turn.setTransform(place);
    turn.drawImage(0, 0, sprite_);
  }
  const bool is_flat = look_ == DrawingLook::kSmoothFlat;
  const bool cuts = has_hard_edges_ && look_ != DrawingLook::kCrisp;
  for (int y = 0; y < shown.height() && (is_flat || cuts); ++y) {
    for (int x = 0; x < shown.width(); ++x) {
      QColor pixel = shown.pixelColor(x, y);
      const bool is_inside = pixel.alpha() >= 250;
      if (is_flat && is_inside) {
        // Fills keep the drawing's own color: snap to the nearest ink.
        pixel = pixel.lightness() > 150   ? QColor(0xff, 0xd0, 0xd8)
                : pixel.lightness() < 60 ? QColor(0x2a, 0x10, 0x18)
                                          : theme::kPick;
      }
      if (cuts) {
        pixel.setAlpha(pixel.alpha() >= kCut ? 255 : 0);
      }
      shown.setPixelColor(x, y, pixel);
    }
  }
  painter.drawImage(0, 0, shown);
}

}  // namespace snapper
