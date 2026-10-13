#include "render/reference_paint.h"

#include <QColor>
#include <QTransform>

#include <cassert>

namespace snapper {
namespace {

// Like a light table: the lines come out in one clear color.
constexpr QColor kBlueTint{40, 110, 255};
constexpr QColor kRedTint{235, 50, 60};

}  // namespace

QImage TintedPicture(const QImage& picture, ReferenceTint tint) {
  assert(!picture.isNull());
  assert(static_cast<int>(tint) < kReferenceTintCount);
  const bool is_plain = tint == ReferenceTint::kNone;
  if (is_plain) {
    return picture;
  }
  const QImage source =
      picture.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  QImage grey = source.convertToFormat(QImage::Format_Grayscale8)
                    .convertToFormat(QImage::Format_ARGB32_Premultiplied);
  QPainter painter(&grey);
  // Screen lifts black to the tint and leaves white white.
  painter.setCompositionMode(QPainter::CompositionMode_Screen);
  painter.fillRect(grey.rect(),
                   tint == ReferenceTint::kBlue ? kBlueTint : kRedTint);
  // Then the picture's own see-through parts come back.
  painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  painter.drawImage(0, 0, source);
  painter.end();
  assert(grey.size() == picture.size());
  return grey;
}

void PaintReference(const Reference& reference, const QImage& picture,
                    QSize canvas, double scale, bool is_ended,
                    QPainter* painter) {
  assert(painter != nullptr);
  assert(scale > 0.0);
  const bool is_drawable = !picture.isNull() && reference.opacity > 0.0;
  if (!is_drawable) {
    return;
  }
  const QRectF place = ReferenceRect(reference, canvas);
  const QRectF target(place.topLeft() * scale, place.size() * scale);
  painter->save();
  painter->setOpacity(reference.opacity * (is_ended ? 0.5 : 1.0));
  painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
  if (reference.is_flipped) {
    const double middle = target.center().x();
    painter->setTransform(QTransform::fromTranslate(middle, 0.0)
                              .scale(-1.0, 1.0)
                              .translate(-middle, 0.0),
                          true);
  }
  painter->drawImage(target, TintedPicture(picture, reference.tint));
  painter->restore();
}

}  // namespace snapper
