#include "render/line_brush.h"

#include <QPainterPath>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "anim/vector_path.h"

namespace snapper {
namespace {

// The most dabs one curve gets, so a huge line stays quick.
constexpr int kMaxDabs = 20000;

// A repeatable number from 0 to 1 for dab index on curve: grain that
// stays put from frame to frame.
double Grain(int curve, int index, int salt) {
  assert(index >= 0);
  assert(curve >= 0);
  uint32_t h = static_cast<uint32_t>(curve) * 2654435761u ^
               static_cast<uint32_t>(index) * 2246822519u ^
               static_cast<uint32_t>(salt) * 3266489917u;
  h ^= h >> 15;
  h *= 2246822519u;
  h ^= h >> 13;
  return static_cast<double>(h & 0xffffu) / 65535.0;
}

// One stretch of a curve between two points, with their widths.
struct Stretch final {
  QPainterPath path;
  double from_width = 1.0;
  double to_width = 1.0;
};

std::vector<Stretch> StretchesOf(const VectorDrawing& drawing,
                                 const VectorCurve& curve,
                                 std::span<const QPointF> places) {
  assert(places.size() == drawing.points.size());
  assert(curve.points.size() <= static_cast<size_t>(kMaxCurvePoints));
  std::vector<Stretch> stretches;
  const int count = static_cast<int>(curve.points.size());
  const int segments = curve.is_closed && count > 2 ? count : count - 1;
  for (int s = 0; s < segments; ++s) {
    const VectorPoint* a =
        FindPoint(drawing, curve.points[static_cast<size_t>(s)]);
    const VectorPoint* b =
        FindPoint(drawing, curve.points[static_cast<size_t>((s + 1) % count)]);
    stretches.push_back({SegmentPath(drawing, curve, s, places),
                         a != nullptr ? a->width : 1.0,
                         b != nullptr ? b->width : 1.0});
  }
  return stretches;
}

// The dab the brush lays at progress u (0 to 1 along the curve) for a
// line of width, or nothing (a gap). Answers its diameter and opacity.
struct Dab final {
  double diameter = 0.0;
  double opacity = 0.0;
  QPointF nudge;
};

Dab DabOf(LineBrush brush, double width, double u, int curve, int index) {
  assert(width >= 0.0);
  assert(u >= -1e-9 && u <= 1.0 + 1e-9);
  const double grain = Grain(curve, index, 1);
  switch (brush) {
    case LineBrush::kSolid:
      return {width, 1.0, {}};
    case LineBrush::kPencil:
      return {width * 0.6, 0.35 + 0.45 * grain,
              QPointF(Grain(curve, index, 2) - 0.5,
                      Grain(curve, index, 3) - 0.5) *
                  width * 0.35};
    case LineBrush::kInk: {
      const double swell = std::sin(std::numbers::pi * u);
      return {width * (0.2 + 0.9 * swell), 1.0, {}};
    }
    case LineBrush::kMarker:
      return {width, 0.08, {}};
    case LineBrush::kChalk: {
      const bool is_gap = grain < 0.18;
      return {width * (0.5 + 0.5 * Grain(curve, index, 4)),
              is_gap ? 0.0 : 0.25 + 0.5 * Grain(curve, index, 5),
              QPointF(Grain(curve, index, 6) - 0.5,
                      Grain(curve, index, 7) - 0.5) *
                  width * 0.8};
    }
    case LineBrush::kDots:
      return {width, 1.0, {}};
  }
  return {width, 1.0, {}};
}

// How far apart dabs sit along the line, by brush and width.
double Spacing(LineBrush brush, double width) {
  assert(width >= 0.0);
  const double step = brush == LineBrush::kDots    ? width * 2.2
                      : brush == LineBrush::kChalk ? width * 0.35
                      : brush == LineBrush::kMarker
                          ? width * 0.12
                          : width * 0.2;
  return std::max(step, 0.5);
}

void PaintCurve(const VectorDrawing& drawing, const VectorShape& shape,
                int curve_index, std::span<const QPointF> places,
                QPainter* painter) {
  assert(painter != nullptr);
  assert(curve_index >= 0);
  const VectorCurve& curve =
      drawing.curves[static_cast<size_t>(curve_index)];
  const std::vector<Stretch> stretches = StretchesOf(drawing, curve, places);
  double total = 0.0;
  for (const Stretch& stretch : stretches) {
    total += stretch.path.length();
  }
  if (total <= 0.0) {
    return;
  }
  QColor color = shape.stroke;
  const double base_alpha = color.alphaF();
  int index = 0;
  double walked = 0.0;
  for (const Stretch& stretch : stretches) {
    const double length = stretch.path.length();
    double along = 0.0;
    while (along <= length && index < kMaxDabs) {
      const double t = length > 0.0 ? along / length : 0.0;
      const double width =
          shape.stroke_width *
          (stretch.from_width + (stretch.to_width - stretch.from_width) * t);
      const double u = std::clamp((walked + along) / total, 0.0, 1.0);
      const Dab dab = DabOf(shape.brush, width, u, curve_index, index);
      if (dab.opacity > 0.0 && dab.diameter > 0.0) {
        const QPointF at =
            stretch.path.pointAtPercent(stretch.path.percentAtLength(along)) +
            dab.nudge;
        color.setAlphaF(static_cast<float>(base_alpha * dab.opacity));
        painter->setBrush(color);
        painter->drawEllipse(at, dab.diameter / 2.0, dab.diameter / 2.0);
      }
      along += Spacing(shape.brush, std::max(width, 0.5));
      ++index;
    }
    walked += length;
  }
}

}  // namespace

void PaintBrushedStroke(const VectorDrawing& drawing, const VectorShape& shape,
                        std::span<const QPointF> places, QPainter* painter) {
  assert(painter != nullptr);
  assert(places.size() == drawing.points.size());
  painter->save();
  painter->setPen(Qt::NoPen);
  for (int curve : shape.curves) {
    PaintCurve(drawing, shape, curve, places, painter);
  }
  painter->restore();
}

}  // namespace snapper
