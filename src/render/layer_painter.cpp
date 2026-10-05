#include "render/layer_painter.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cassert>
#include <variant>

#include "anim/doll_pose.h"
#include "anim/sampler.h"
#include "render/warp_raster.h"

namespace snapper {
namespace {

// Each content kind draws itself; PaintLayer picks by visiting.
struct Painter final {
  const Project& project;
  Frame frame;
  const QTransform& world;
  ImageCache* cache;
  QPainter* painter;

  void operator()(const DollLayer& layer) const;
  void operator()(const ImageLayer& layer) const;
  void operator()(const TextLayer& layer) const;
  void operator()(const EffectLayer& layer) const;
};

void Painter::operator()(const DollLayer& layer) const {
  assert(cache != nullptr && painter != nullptr);
  assert(frame.index() >= 0);
  const Doll* doll = FindDoll(project, layer.doll);
  const bool has_doll = doll != nullptr;
  if (!has_doll) {
    return;
  }
  const double layer_opacity = painter->opacity();
  for (const PlacedPiece& piece :
       PlaceDoll(*doll, SamplePoses(layer, frame))) {
    const QImage& image = cache->Get(piece.drawing);
    const bool is_drawable = !image.isNull() && piece.opacity > 0.0;
    if (!is_drawable) {
      continue;
    }
    painter->setOpacity(layer_opacity * piece.opacity);
    const bool is_warped = !piece.warp_points.empty();
    if (is_warped) {
      const WarpedImage warped =
          WarpImage(image, piece.grid, piece.warp_points);
      painter->setTransform(QTransform::fromTranslate(warped.origin.x(),
                                                      warped.origin.y()) *
                            piece.transform * world);
      painter->drawImage(0, 0, warped.image);
    } else {
      painter->setTransform(piece.transform * world);
      painter->drawImage(QRectF(QPointF(), QSizeF(piece.size)), image);
    }
  }
  painter->setOpacity(layer_opacity);
}

void Painter::operator()(const ImageLayer& layer) const {
  assert(cache != nullptr && painter != nullptr);
  assert(frame.index() >= 0);
  const QImage& image = cache->Get(layer.path);
  const bool has_image = !image.isNull();
  if (!has_image) {
    return;
  }
  painter->setTransform(world);
  painter->drawImage(QPointF(-image.width() / 2.0, -image.height() / 2.0),
                     image);
}

void Painter::operator()(const TextLayer& layer) const {
  assert(painter != nullptr);
  assert(layer.size > 0.0);
  QFont font(layer.font_family);
  font.setPixelSize(std::max(1, static_cast<int>(layer.size)));
  font.setBold(layer.is_bold);
  const QFontMetricsF metrics(font);
  const QStringList lines = layer.text.split(QLatin1Char('\n'));
  QPainterPath path;
  double y = metrics.ascent();
  for (const QString& line : lines) {
    path.addText(-metrics.horizontalAdvance(line) / 2.0, y, font, line);
    y += metrics.lineSpacing();
  }
  // Centred on the layer's origin.
  path.translate(0, -(y - metrics.lineSpacing() + metrics.descent()) / 2.0);
  painter->setTransform(world);
  const bool has_outline = layer.outline_width > 0.0;
  if (has_outline) {
    // The stroke straddles the edge; doubling keeps the outer half the
    // width asked for.
    painter->strokePath(path, QPen(layer.outline, layer.outline_width * 2.0,
                                   Qt::SolidLine, Qt::RoundCap,
                                   Qt::RoundJoin));
  }
  painter->fillPath(path, layer.fill);
}

void Painter::operator()(const EffectLayer& layer) const {
  assert(painter != nullptr);
  assert(static_cast<int>(layer.kind) < kEffectKindCount);
}

}  // namespace

QTransform LayerTransform(const Layer& layer, Frame frame) {
  assert(frame.index() >= 0);
  assert(IsSorted(layer.transform));
  const PiecePose pose = Sample(layer.transform, frame, PiecePose());
  const auto* doll = std::get_if<DollLayer>(&layer.content);
  const bool is_flipped = doll != nullptr && doll->is_flipped;
  const QTransform flip =
      is_flipped ? QTransform::fromScale(-1.0, 1.0) : QTransform();
  return flip * PoseMatrix(pose, QPointF());
}

void PaintLayer(const Project& project, const Layer& layer, Frame frame,
                const QTransform& view, ImageCache* cache,
                QPainter* painter) {
  assert(cache != nullptr);
  assert(painter != nullptr && painter->isActive());
  const PiecePose pose = Sample(layer.transform, frame, PiecePose());
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::SmoothPixmapTransform);
  painter->setOpacity(std::clamp(pose.opacity, 0.0, 1.0));
  const QTransform world = LayerTransform(layer, frame) * view;
  std::visit(Painter{project, frame, world, cache, painter}, layer.content);
  painter->restore();
}

}  // namespace snapper
