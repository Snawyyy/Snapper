#include "render/character_paint.h"

#include <QDir>
#include <QPainterPathStroker>
#include <QPen>

#include <algorithm>
#include <cassert>

#include "anim/vector_path.h"
#include "render/line_brush.h"

namespace snapper {
namespace {

// True when every point of the shape's curves has the full width.
bool IsEvenWidth(const VectorDrawing& drawing, const VectorShape& shape) {
  assert(shape.curves.size() <= static_cast<size_t>(kMaxVectorCurves));
  assert(drawing.curves.size() <= static_cast<size_t>(kMaxVectorCurves));
  return std::ranges::all_of(shape.curves, [&drawing](int curve) {
    const VectorCurve& line = drawing.curves[static_cast<size_t>(curve)];
    return std::ranges::all_of(line.points, [&drawing](PointId id) {
      const VectorPoint* point = FindPoint(drawing, id);
      return point == nullptr || point->width == 1.0;
    });
  });
}

QPen StrokePen(const VectorShape& shape, double width) {
  assert(width >= 0.0);
  assert(shape.stroke_width >= 0.0);
  return QPen(shape.stroke, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

// A stroke whose width changes along it: each stretch between two
// points is drawn with the width halfway between theirs.
void PaintTaperedStroke(const VectorDrawing& drawing, const VectorShape& shape,
                        std::span<const QPointF> places, QPainter* painter) {
  assert(painter != nullptr);
  assert(places.size() == drawing.points.size());
  for (int curve : shape.curves) {
    const VectorCurve& line = drawing.curves[static_cast<size_t>(curve)];
    const QPainterPath path = CurvePath(drawing, line, places);
    const int count = static_cast<int>(line.points.size());
    for (int s = 0; s < path.elementCount() - 1 && count > 1; s += 3) {
      if (s + 3 >= path.elementCount()) {
        break;
      }
      const int from = (s / 3) % count;
      const int to = (s / 3 + 1) % count;
      const VectorPoint* a = FindPoint(drawing, line.points[static_cast<size_t>(from)]);
      const VectorPoint* b = FindPoint(drawing, line.points[static_cast<size_t>(to)]);
      const double width =
          shape.stroke_width * ((a ? a->width : 1.0) + (b ? b->width : 1.0)) / 2.0;
      QPainterPath piece(path.elementAt(s));
      piece.cubicTo(path.elementAt(s + 1), path.elementAt(s + 2),
                    path.elementAt(s + 3));
      painter->strokePath(piece, StrokePen(shape, width));
    }
  }
}

void PaintVector(const Character& character, const CharacterPart& part,
                 const PosedPart& posed, QPainter* painter) {
  assert(painter != nullptr);
  assert(posed.points.size() == part.drawing.points.size());
  const VectorDrawing& drawing = part.drawing;
  for (const VectorShape& own : drawing.shapes) {
    const VectorShape shape = LookOf(character, own);
    if (shape.has_fill) {
      painter->fillPath(ShapeFillPath(drawing, shape, posed.points), shape.fill);
    }
    if (!shape.has_stroke || shape.stroke_width <= 0.0) {
      continue;
    }
    if (shape.brush != LineBrush::kSolid) {
      PaintBrushedStroke(drawing, shape, posed.points, painter);
    } else if (IsEvenWidth(drawing, shape)) {
      painter->strokePath(ShapeStrokePath(drawing, shape, posed.points),
                          StrokePen(shape, shape.stroke_width));
    } else {
      PaintTaperedStroke(drawing, shape, posed.points, painter);
    }
  }
}

void PaintImage(const Character& character, const CharacterPart& part,
                const PosedPart& posed, const QTransform& world,
                ImageCache* cache, QPainter* painter) {
  assert(cache != nullptr);
  assert(painter != nullptr);
  const QImage& image = cache->Get(QDir(character.folder).filePath(part.image));
  if (image.isNull()) {
    return;
  }
  painter->setTransform(posed.to_character * world);
  painter->drawImage(QRectF(QPointF(), part.image_size.isEmpty()
                                           ? QSizeF(image.size())
                                           : part.image_size),
                     image);
}

}  // namespace

void PaintCharacter(const Character& character, const PosedCharacter& posed,
                    const QTransform& world, ImageCache* cache,
                    QPainter* painter) {
  assert(cache != nullptr);
  assert(painter != nullptr && painter->isActive());
  const double opacity = painter->opacity();
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::SmoothPixmapTransform);
  for (const PosedPart& shown : posed.parts) {
    const CharacterPart* part = FindPart(character, shown.id);
    if (part == nullptr || shown.opacity <= 0.0) {
      continue;
    }
    painter->setOpacity(opacity * std::clamp(shown.opacity, 0.0, 1.0));
    if (part->kind == PartKind::kVector) {
      painter->setTransform(world);
      PaintVector(character, *part, shown, painter);
    } else if (part->kind == PartKind::kImage) {
      PaintImage(character, *part, shown, world, cache, painter);
    }
  }
  painter->restore();
}

QPainterPath PartOutline(const Character& character, const PosedPart& shown) {
  assert(shown.id.IsValid());
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  const CharacterPart* part = FindPart(character, shown.id);
  QPainterPath outline;
  if (part == nullptr) {
    return outline;
  }
  if (part->kind == PartKind::kImage) {
    QPainterPath box;
    box.addRect(QRectF(QPointF(), part->image_size));
    return shown.to_character.map(box);
  }
  QPainterPathStroker stroker;
  for (const VectorShape& own : part->drawing.shapes) {
    const VectorShape shape = LookOf(character, own);
    if (shape.has_fill) {
      outline.addPath(ShapeFillPath(part->drawing, shape, shown.points));
    }
    stroker.setWidth(std::max(6.0, shape.stroke_width));
    outline.addPath(stroker.createStroke(
        ShapeStrokePath(part->drawing, shape, shown.points)));
  }
  return outline;
}

std::optional<PartId> HitPart(const Character& character,
                              const PosedCharacter& posed, QPointF at) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(posed.parts.size() <= character.parts.size());
  for (auto it = posed.parts.rbegin(); it != posed.parts.rend(); ++it) {
    if (it->opacity > 0.0 && PartOutline(character, *it).contains(at)) {
      return it->id;
    }
  }
  return std::nullopt;
}

}  // namespace snapper
