#include "render/layer_painter.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>

#include "anim/layer_transform.h"
#include "anim/sampler.h"
#include "render/character_paint.h"

namespace snapper {
namespace {

// Each content kind draws itself; PaintLayer picks by visiting.
struct Painter final {
  const Project& project;
  Frame frame;
  const QTransform& world;
  ImageCache* cache;
  QPainter* painter;

  void operator()(const CharacterLayer& layer) const;
  void operator()(const ImageLayer& layer) const;
  void operator()(const TextLayer& layer) const;
  void operator()(const EffectLayer& layer) const;
  void operator()(const StoryboardLayer& layer) const;
};

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
  painter->setTransform(world);
  PaintLettering(TextPath(layer), layer, painter);
}

void Painter::operator()(const CharacterLayer& layer) const {
  assert(cache != nullptr && painter != nullptr);
  assert(frame.index() >= 0);
  const Character* character = FindCharacter(project, layer.character);
  if (character == nullptr) {
    return;
  }
  const PosedCharacter posed =
      PoseCharacter(*character, PoseAt(*character, layer, frame));
  PaintCharacter(*character, posed, world, cache, painter);
}

// Effects change what is drawn instead; RenderShot applies them.
void Painter::operator()([[maybe_unused]] const EffectLayer& layer) const {
  assert(painter != nullptr);
  assert(static_cast<int>(layer.kind) < kEffectKindCount);
}

// A storyboard sheet lies over the canvas, not in the camera's world:
// FrameRenderer::PaintBoardLayer draws it.
void Painter::operator()(
    [[maybe_unused]] const StoryboardLayer& layer) const {
  assert(painter != nullptr);
  assert(layer.opacity >= 0.0 && layer.opacity <= 1.0);
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

QPainterPath TextPath(const TextLayer& text) {
  assert(text.size > 0.0);
  assert(!text.font_family.isNull());
  const QFont font = TextFont(text);
  const QFontMetricsF metrics(font);
  const QRectF box = TextBox(text);
  QPainterPath path;
  double y = box.top() + metrics.ascent();
  // addText lays each line out with Qt's text layout: right-to-left
  // runs, mixed directions and per-letter font fallback come with it.
  for (const QString& line : text.text.split(QLatin1Char('\n'))) {
    path.addText(-metrics.horizontalAdvance(line) / 2.0, y, font, line);
    y += metrics.lineSpacing();
  }
  return path;
}

void PaintLettering(const QPainterPath& path, const TextLayer& text,
                    QPainter* painter) {
  assert(painter != nullptr);
  assert(text.outline_width >= 0.0);
  const bool has_outline = text.outline_width > 0.0;
  if (has_outline) {
    // The stroke straddles the edge; doubling keeps the outer half the
    // width asked for.
    painter->strokePath(path, QPen(text.outline, text.outline_width * 2.0,
                                   Qt::SolidLine, Qt::RoundCap,
                                   Qt::RoundJoin));
  }
  painter->fillPath(path, text.fill);
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

void PaintLayer(const Project& project, const Layer& layer, Frame frame,
                const QTransform& view, LinkSolver* links, ImageCache* cache,
                QPainter* painter) {
  assert(cache != nullptr);
  assert(painter != nullptr && painter->isActive());
  const LayerPose pose = Sample(layer.transform, frame, LayerPose());
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::SmoothPixmapTransform);
  painter->setOpacity(std::clamp(pose.opacity, 0.0, 1.0));
  const QTransform world = (links != nullptr
                                ? links->LayerTransform(layer, frame)
                                : LayerTransform(layer, frame)) *
                           view;
  std::visit(Painter{project, frame, world, cache, painter}, layer.content);
  painter->restore();
}

}  // namespace snapper
