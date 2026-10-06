#include "render/stage_hit.h"

#include <QLineF>
#include <QPainterPath>
#include <QTransform>

#include <cassert>
#include <cmath>
#include <variant>

#include "anim/doll_lean.h"
#include "anim/doll_pose.h"
#include "render/frame_renderer.h"
#include "render/layer_painter.h"
#include "render/stage_geometry.h"
#include "render/warp_raster.h"

namespace snapper {
namespace {

// Pixels fainter than this are see-through to clicks.
constexpr int kSolidAlpha = 32;

// True when point (in image pixels) lands on a solid pixel of image.
bool IsSolidAt(const QImage& image, QPointF point) {
  assert(!image.isNull());
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  const int x = static_cast<int>(std::floor(point.x()));
  const int y = static_cast<int>(std::floor(point.y()));
  const bool is_inside =
      x >= 0 && y >= 0 && x < image.width() && y < image.height();
  return is_inside && qAlpha(image.pixel(x, y)) >= kSolidAlpha;
}

}  // namespace

std::optional<QString> HitDollPiece(const Doll& doll, const PoseMap& poses,
                                    const QTransform& world, QPointF point,
                                    ImageCache* cache) {
  assert(cache != nullptr);
  assert(poses.size() <= static_cast<size_t>(kMaxPieceTracks));
  const auto placed = PlaceDoll(doll, poses);
  for (auto it = placed.rbegin(); it != placed.rend(); ++it) {
    const QImage& image = cache->Get(it->drawing);
    const bool is_drawn = !image.isNull() && it->opacity > 0.0;
    if (!is_drawn) {
      continue;
    }
    bool is_invertible = false;
    const QTransform back = (it->transform * world).inverted(&is_invertible);
    if (!is_invertible) {
      continue;
    }
    const QPointF in_drawing = back.map(point);
    const bool is_warped = !it->warp_points.empty();
    const WarpedImage warped =
        is_warped ? WarpImage(image, it->grid, it->warp_points)
                  : WarpedImage{QImage(), QPointF()};
    const bool is_hit =
        is_warped ? IsSolidAt(warped.image, in_drawing - warped.origin)
                  : IsSolidAt(image, QPointF(in_drawing.x() * image.width() /
                                                 it->size.width(),
                                             in_drawing.y() * image.height() /
                                                 it->size.height()));
    if (is_hit) {
      return it->name;
    }
  }
  return std::nullopt;
}

namespace {

// Hit-tests one layer by its kind. An empty name means the whole layer
// was hit; nothing means it wasn't.
struct LayerHitter final {
  const Project& project;
  const Layer& whole;
  Frame local;
  const QTransform& world;
  QPointF point;
  QPointF in_layer;
  ImageCache* cache;

  std::optional<QString> operator()(const DollLayer& layer) const {
    assert(cache != nullptr);
    assert(!layer.doll.isNull());
    const Doll* doll = FindDoll(project, layer.doll);
    const bool has_doll = doll != nullptr;
    return has_doll ? HitDollPiece(*doll, ShownPoses(*doll, whole, local),
                                   world, point, cache)
                    : std::nullopt;
  }
  std::optional<QString> operator()(const ImageLayer& layer) const {
    assert(cache != nullptr);
    assert(!layer.path.isNull());
    const QImage& pixels = cache->Get(layer.path);
    const QPointF corner(pixels.width() / 2.0, pixels.height() / 2.0);
    const bool is_hit =
        !pixels.isNull() && IsSolidAt(pixels, in_layer + corner);
    return is_hit ? std::optional<QString>(QString()) : std::nullopt;
  }
  std::optional<QString> operator()(const TextLayer& layer) const {
    assert(layer.size > 0.0);
    assert(std::isfinite(in_layer.x()));
    const bool is_hit = TextBox(layer).contains(in_layer);
    return is_hit ? std::optional<QString>(QString()) : std::nullopt;
  }
  // Effects change pixels; there is nothing of theirs to click.
  std::optional<QString> operator()(const EffectLayer& layer) const {
    assert(static_cast<int>(layer.kind) < kEffectKindCount);
    assert(cache != nullptr);
    return std::nullopt;
  }
};

}  // namespace

std::optional<StageHit> HitTest(const Project& project, const Shot& shot,
                                Frame local, QPointF point, double scale,
                                ImageCache* cache) {
  assert(cache != nullptr);
  assert(scale > 0.0);
  const QTransform view =
      FrameRenderer::ViewTransform(project, shot, local, scale);
  for (auto it = shot.layers.rbegin(); it != shot.layers.rend(); ++it) {
    const QTransform world = LayerTransform(*it, local) * view;
    bool is_invertible = false;
    const QPointF in_layer = world.inverted(&is_invertible).map(point);
    const bool is_testable =
        IsLayerLive(*it, local, shot.length) && is_invertible;
    if (!is_testable) {
      continue;
    }
    const std::optional<QString> hit = std::visit(
        LayerHitter{project, *it, local, world, point, in_layer, cache},
        it->content);
    const bool is_hit = hit.has_value();
    if (is_hit) {
      return StageHit{it->id, *hit};
    }
  }
  return std::nullopt;
}

QPolygonF LayerShape(const Project& project, const Shot& shot, LayerId layer,
                     Frame local, double scale, ImageCache* cache) {
  assert(cache != nullptr);
  assert(scale > 0.0);
  const Layer* found = layer.IsValid() ? FindLayer(shot, layer) : nullptr;
  const auto to_screen = LayerToScreen(project, shot, layer, local, scale);
  const bool is_found = found != nullptr && to_screen.has_value();
  if (!is_found) {
    return QPolygonF();
  }
  const auto* text = std::get_if<TextLayer>(&found->content);
  const auto* image = std::get_if<ImageLayer>(&found->content);
  const auto* posed = std::get_if<DollLayer>(&found->content);
  const QSizeF picture =
      image != nullptr ? QSizeF(cache->Get(image->path).size()) : QSizeF();
  QRectF own =
      text != nullptr
          ? TextBox(*text)
          : QRectF(QPointF(-picture.width() / 2, -picture.height() / 2),
                   picture);
  const Doll* doll =
      posed != nullptr ? FindDoll(project, posed->doll) : nullptr;
  const bool is_doll = doll != nullptr;
  if (is_doll) {
    // One box around every posed piece, in the layer's own space, so it
    // turns and scales with the doll.
    own = QRectF();
    for (const PlacedPiece& piece :
         PlaceDoll(*doll, ShownPoses(*doll, *found, local))) {
      own = own.united(
          piece.transform.mapRect(QRectF(QPointF(), QSizeF(piece.size))));
    }
  }
  return to_screen->map(QPolygonF(own));
}

namespace {

// from pushed on past to, by kTurnGap pixels.
QPointF PastBy(QPointF from, QPointF to) {
  assert(std::isfinite(from.x()) && std::isfinite(to.x()));
  const QLineF out(from, to);
  const bool has_length = out.length() > 0.0;
  assert(std::isfinite(out.length()));
  return has_length ? to + (to - from) * (kTurnGap / out.length())
                    : to + QPointF(kTurnGap, 0.0);
}

}  // namespace

std::optional<TurnHandles> TurnHandlesOf(const Project& project,
                                         const Shot& shot, LayerId layer,
                                         Frame local, double scale,
                                         ImageCache* cache) {
  assert(cache != nullptr);
  assert(scale > 0.0);
  const Layer* found = layer.IsValid() ? FindLayer(shot, layer) : nullptr;
  const bool is_doll =
      found != nullptr && std::holds_alternative<DollLayer>(found->content);
  const QPolygonF box =
      is_doll ? LayerShape(project, shot, layer, local, scale, cache)
              : QPolygonF();
  // A rectangle's outline runs top-left, top-right, bottom-right,
  // bottom-left.
  const bool has_box = box.size() >= 4;
  if (!has_box) {
    return std::nullopt;
  }
  // Set out past the box, so grabbing the doll near its edge still
  // moves it.
  return TurnHandles{PastBy((box[0] + box[3]) / 2.0, (box[1] + box[2]) / 2.0),
                     PastBy((box[0] + box[1]) / 2.0,
                            (box[2] + box[3]) / 2.0)};
}

std::vector<StageHit> HitBox(const Project& project, const Shot& shot,
                             Frame local, const QRectF& box, double scale,
                             ImageCache* cache) {
  assert(scale > 0.0 && cache != nullptr);
  assert(box.isValid());
  std::vector<StageHit> hits;
  QPainterPath area;
  area.addRect(box);
  for (const Layer& layer : shot.layers) {
    const bool is_live = IsLayerLive(layer, local, shot.length) &&
                         !std::holds_alternative<EffectLayer>(layer.content);
    if (!is_live) {
      continue;
    }
    const auto* posed = std::get_if<DollLayer>(&layer.content);
    const Doll* doll =
        posed != nullptr ? FindDoll(project, posed->doll) : nullptr;
    const bool is_doll = doll != nullptr;
    if (is_doll) {
      for (const RigPiece& piece : doll->rig.pieces) {
        const auto outline =
            PieceOnScreen(project, shot, layer.id, piece.name, local, scale);
        QPainterPath shape;
        shape.addPolygon(outline ? outline->outline : QPolygonF());
        const bool is_caught = outline.has_value() && area.intersects(shape);
        if (is_caught) {
          hits.push_back({layer.id, piece.name});
        }
      }
      continue;
    }
    QPainterPath shape;
    shape.addPolygon(LayerShape(project, shot, layer.id, local, scale, cache));
    const bool is_caught = area.intersects(shape);
    if (is_caught) {
      hits.push_back({layer.id, QString()});
    }
  }
  return hits;
}

}  // namespace snapper
