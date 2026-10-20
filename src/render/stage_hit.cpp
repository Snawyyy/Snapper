#include "render/stage_hit.h"

#include <QPainterPath>
#include <QTransform>

#include <cassert>
#include <cmath>
#include <variant>

#include "anim/layer_order.h"
#include "anim/links.h"
#include "render/character_paint.h"
#include "render/frame_renderer.h"
#include "render/layer_painter.h"
#include "render/stage_geometry.h"

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

// Hit-tests one layer by its kind: true when point lands on it.
struct LayerHitter final {
  const Project& project;
  Frame local;
  QPointF in_layer;
  ImageCache* cache;

  bool operator()(const ImageLayer& layer) const {
    assert(cache != nullptr);
    assert(!layer.path.isNull());
    const QImage& pixels = cache->Get(layer.path);
    const QPointF corner(pixels.width() / 2.0, pixels.height() / 2.0);
    return !pixels.isNull() && IsSolidAt(pixels, in_layer + corner);
  }
  bool operator()(const TextLayer& layer) const {
    assert(layer.size > 0.0);
    assert(std::isfinite(in_layer.x()));
    return TextBox(layer).contains(in_layer);
  }
  // A character is picked whole: its parts pose through its bones.
  bool operator()(const CharacterLayer& layer) const {
    assert(cache != nullptr);
    assert(!layer.character.isNull());
    const Character* character = FindCharacter(project, layer.character);
    if (character == nullptr) {
      return false;
    }
    const PosedCharacter posed =
        PoseCharacter(*character, PoseAt(*character, layer, local));
    return HitPart(*character, posed, in_layer).has_value();
  }
  // Effects change pixels; there is nothing of theirs to click.
  bool operator()(const EffectLayer& layer) const {
    assert(static_cast<int>(layer.kind) < kEffectKindCount);
    assert(cache != nullptr);
    return false;
  }
  // A storyboard sheet is picked on its timeline row or in the layer
  // list, never by clicking the stage through what lies under it.
  bool operator()(const StoryboardLayer& layer) const {
    assert(layer.opacity >= 0.0 && layer.opacity <= 1.0);
    assert(cache != nullptr);
    return false;
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
  LinkSolver links(project, shot);
  const std::vector<const Layer*> drawn = LayersInDrawOrder(shot, local);
  for (auto it = drawn.rbegin(); it != drawn.rend(); ++it) {
    const Layer& layer = **it;
    if (layer.is_locked) {
      continue;
    }
    const QTransform world = links.LayerTransform(layer, local) * view;
    bool is_invertible = false;
    const QPointF in_layer = world.inverted(&is_invertible).map(point);
    if (!is_invertible) {
      continue;
    }
    const bool is_hit = std::visit(
        LayerHitter{project, local, in_layer, cache}, layer.content);
    if (is_hit) {
      return StageHit{layer.id};
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
  if (found == nullptr || !to_screen.has_value()) {
    return QPolygonF();
  }
  const auto* text = std::get_if<TextLayer>(&found->content);
  const auto* image = std::get_if<ImageLayer>(&found->content);
  const QSizeF picture =
      image != nullptr ? QSizeF(cache->Get(image->path).size()) : QSizeF();
  QRectF own =
      text != nullptr
          ? TextBox(*text)
          : QRectF(QPointF(-picture.width() / 2, -picture.height() / 2),
                   picture);
  const auto* cast = std::get_if<CharacterLayer>(&found->content);
  const Character* character =
      cast != nullptr ? FindCharacter(project, cast->character) : nullptr;
  if (character != nullptr) {
    // One box around every shown part, in the layer's own space, so it
    // turns and scales with the character.
    own = QRectF();
    const PosedCharacter shown =
        PoseCharacter(*character, PoseAt(*character, *cast, local));
    for (const PosedPart& part : shown.parts) {
      own = own.united(PartOutline(*character, part).boundingRect());
    }
  }
  return to_screen->map(QPolygonF(own));
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
                         IsStagePicture(layer) && !layer.is_locked;
    if (!is_live) {
      continue;
    }
    QPainterPath shape;
    shape.addPolygon(LayerShape(project, shot, layer.id, local, scale, cache));
    if (area.intersects(shape)) {
      hits.push_back({layer.id});
    }
  }
  return hits;
}

}  // namespace snapper
