#include "render/layer_painter.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "anim/doll_lean.h"
#include "anim/doll_pose.h"
#include "anim/joint_snap.h"
#include "anim/sampler.h"
#include "render/upscale.h"
#include "render/warp_raster.h"

namespace snapper {
namespace {

// How many quarters of a drawing's size are enough to bend for it to
// fill transform's pixels: a doll drawn at half size needs only half
// the pixels each way. Rounds to the nearest quarter, up on a tie;
// never less than a quarter, never more than most.
int DetailFor(const QTransform& transform, int most = kFullDetail) {
  assert(kFullDetail > 1);
  assert(most >= 1);
  const double pixels_per_pixel =
      std::sqrt(std::abs(transform.determinant()));
  assert(pixels_per_pixel >= 0.0);
  const int quarters =
      static_cast<int>(std::ceil(pixels_per_pixel * 4.0 - 0.5));
  return std::clamp(quarters, 1, most);
}

// How piece of doll reads its drawing: Crisp while cache is held to
// Crisp or the piece stays Crisp, else the doll's look.
WarpLook LookOf(const Doll& doll, const QString& piece,
                const ImageCache& cache) {
  assert(static_cast<int>(doll.rig.look) < kDrawingLookCount);
  assert(!piece.isEmpty());
  const RigPiece* rig = FindRig(doll.rig, piece);
  const bool is_crisp =
      cache.is_crisp_only() || (rig != nullptr && rig->stays_crisp);
  return is_crisp ? WarpLook{}
                  : WarpLookFor(doll.rig.look, doll.rig.hard_edges);
}

// Each content kind draws itself; PaintLayer picks by visiting.
struct Painter final {
  const Project& project;
  const Layer& whole;
  Frame frame;
  const QTransform& world;
  LinkSolver* links;
  ImageCache* cache;
  QPainter* painter;
  const std::set<QString>* only_pieces = nullptr;

  void operator()(const DollLayer& layer) const;
  // The piece's joint circle, if on, just under the piece: as wide as
  // image (the drawing it shows) is across the joint.
  void PaintJointCircle(const Doll& doll, const PlacedPiece& piece,
                        const QImage& image) const;
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
       PlaceDoll(*doll, links != nullptr ? links->Poses(*doll, whole, frame)
                                         : ShownPoses(*doll, whole, frame))) {
    const QImage& image = cache->Get(piece.drawing);
    const bool is_chosen =
        only_pieces == nullptr || only_pieces->contains(piece.name);
    const bool is_drawable =
        is_chosen && !image.isNull() && piece.opacity > 0.0;
    if (!is_drawable) {
      continue;
    }
    painter->setOpacity(layer_opacity * piece.opacity);
    PaintJointCircle(*doll, piece, image);
    const QTransform on_screen = piece.transform * world;
    const WarpLook look = LookOf(*doll, piece.name, *cache);
    // A smooth look is blended by the painter too, unless it cuts out.
    painter->setRenderHint(QPainter::SmoothPixmapTransform,
                           look.is_blended && !look.is_hard);
    const bool is_warped = !piece.warp_points.empty();
    if (is_warped) {
      const WarpedImage& warped = cache->Warped(
          piece.drawing, piece.grid, piece.warp_points,
          DetailFor(on_screen, look.is_blended ? kSmoothDetail : kFullDetail),
          look);
      // A small bend is drawn stretched back to the drawing's size.
      painter->setTransform(QTransform::fromScale(1.0 / warped.detail,
                                                  1.0 / warped.detail) *
                            QTransform::fromTranslate(warped.origin.x(),
                                                      warped.origin.y()) *
                            on_screen);
      painter->drawImage(0, 0, warped.image);
    } else {
      // A smooth piece draws its upscaled copy over the same rectangle,
      // so it sits on exactly the same spot.
      const QImage& shown =
          look.is_blended
              ? cache->Upscaled(
                    piece.drawing,
                    UpscaleTimes(image.size(),
                                 std::sqrt(std::abs(on_screen.determinant()))))
              : image;
      painter->setTransform(on_screen);
      painter->drawImage(QRectF(QPointF(), QSizeF(piece.size)), shown);
    }
  }
  painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
  painter->setOpacity(layer_opacity);
}

void Painter::PaintJointCircle(const Doll& doll, const PlacedPiece& piece,
                               const QImage& image) const {
  assert(cache != nullptr && painter != nullptr);
  assert(!image.isNull());
  const RigPiece* rig = FindRig(doll.rig, piece.name);
  const bool is_on = rig != nullptr && rig->joint_circle.is_on;
  if (!is_on) {
    return;
  }
  // Measured, never stored: a swapped drawing gets its own width.
  const double radius = JointRadius(*cache->limbs()->Of(image), rig->pivot);
  if (radius <= 0.0) {
    return;
  }
  const QColor color = rig->joint_circle.color.isValid()
                           ? rig->joint_circle.color
                           : QColor(Qt::black);
  painter->setTransform(piece.transform * world);
  painter->setPen(Qt::NoPen);
  painter->setBrush(color);
  painter->drawEllipse(rig->pivot, radius, radius);
  painter->setBrush(Qt::NoBrush);
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
  const QFont font = TextFont(layer);
  const QFontMetricsF metrics(font);
  const QRectF box = TextBox(layer);
  QPainterPath path;
  double y = box.top() + metrics.ascent();
  for (const QString& line : layer.text.split(QLatin1Char('\n'))) {
    path.addText(-metrics.horizontalAdvance(line) / 2.0, y, font, line);
    y += metrics.lineSpacing();
  }
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

// Effects change what is drawn instead; RenderShot applies them.
void Painter::operator()([[maybe_unused]] const EffectLayer& layer) const {
  assert(painter != nullptr);
  assert(static_cast<int>(layer.kind) < kEffectKindCount);
}

}  // namespace

QFont TextFont(const TextLayer& text) {
  assert(text.size > 0.0);
  assert(!text.font_family.isNull());
  QFont font(text.font_family);
  font.setPixelSize(std::max(1, static_cast<int>(text.size)));
  font.setBold(text.is_bold);
  return font;
}

QRectF TextBox(const TextLayer& text) {
  assert(text.size > 0.0);
  const QFontMetricsF metrics(TextFont(text));
  const QStringList lines = text.text.split(QLatin1Char('\n'));
  assert(!lines.isEmpty());
  double width = 0.0;
  for (const QString& line : lines) {
    width = std::max(width, metrics.horizontalAdvance(line));
  }
  const double height =
      metrics.lineSpacing() * static_cast<double>(lines.size() - 1) +
      metrics.height();
  return QRectF(-width / 2.0, -height / 2.0, width, height);
}

Layer RestLayer(const QString& doll) {
  assert(!doll.isEmpty());
  Layer layer;
  layer.id = LayerId(1);
  layer.content = DollLayer{doll, {}, false};
  assert(layer.transform.keys.empty());
  return layer;
}

void PaintLayer(const Project& project, const Layer& layer, Frame frame,
                const QTransform& view, LinkSolver* links, ImageCache* cache,
                QPainter* painter, const std::set<QString>* only_pieces) {
  assert(cache != nullptr);
  assert(painter != nullptr && painter->isActive());
  const PiecePose pose = Sample(layer.transform, frame, PiecePose());
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
  painter->setOpacity(std::clamp(pose.opacity, 0.0, 1.0));
  const QTransform world = (links != nullptr
                                ? links->LayerTransform(layer, frame)
                                : LayerTransform(layer, frame)) *
                           view;
  std::visit(Painter{project, layer, frame, world, links, cache, painter,
                     only_pieces},
             layer.content);
  painter->restore();
}

}  // namespace snapper
