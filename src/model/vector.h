#ifndef SNAPPER_MODEL_VECTOR_H_
#define SNAPPER_MODEL_VECTOR_H_

#include <QColor>
#include <QPointF>

#include <vector>

#include "base/id.h"

namespace snapper {

struct PointTag;
// A vector point, unique inside its character.
using PointId = Id<PointTag>;

constexpr int kMaxVectorPoints = 4000;
constexpr int kMaxVectorCurves = 1000;
constexpr int kMaxVectorShapes = 500;
constexpr int kMaxCurvePoints = 1000;
constexpr int kMaxStrokeWidth = 400;

// How round a curve passes through a point, as in Moho: 0 is a sharp
// corner, kSmoothCurvature an even round, more than that bulges. The
// handles are worked out from the neighbours each frame, so a point bent
// by bones keeps a round line without handles to fix.
constexpr double kSmoothCurvature = 0.33;
constexpr double kMaxCurvature = 1.0;

// One point of a vector drawing, in its layer's space (pixels, y down).
// Several curves may pass through one point: they are welded there.
struct VectorPoint final {
  PointId id{};
  QPointF at{};
  double curvature = kSmoothCurvature;
  // Stroke width here, times the shape's stroke width (a line that
  // tapers to 0 at its ends).
  double width = 1.0;

  bool operator==(const VectorPoint&) const = default;
};

// A line through points, in order. A closed curve joins its last point
// back to its first.
struct VectorCurve final {
  std::vector<PointId> points{};
  bool is_closed = false;

  bool operator==(const VectorCurve&) const = default;
};

// How a shape's line is laid down. Solid is one even stroke; the others
// are dabbed along it by the renderer (render/line_brush.h).
enum class LineBrush {
  kSolid,
  // Thin and grainy, like graphite.
  kPencil,
  // Swells in the middle of each line and thins to its ends, like a pen
  // pressed harder mid-stroke.
  kInk,
  // Flat and see-through, darker where it crosses itself.
  kMarker,
  // Broken and dusty, with gaps.
  kChalk,
  // A row of round dots.
  kDots,
};
inline constexpr int kLineBrushCount = 6;

// A fill and a stroke over curves. Closed curves are filled even-odd,
// so a curve inside another cuts a hole. A shape without fill is a line.
struct VectorShape final {
  // Curve indices into the drawing's curves.
  std::vector<int> curves{};
  bool has_fill = true;
  QColor fill = QColor(230, 230, 230);
  bool has_stroke = true;
  QColor stroke = QColor(20, 20, 24);
  double stroke_width = 3.0;
  LineBrush brush = LineBrush::kSolid;

  bool operator==(const VectorShape&) const = default;
};

// What a vector layer draws. Shapes draw in order, first at the back.
struct VectorDrawing final {
  std::vector<VectorPoint> points{};
  std::vector<VectorCurve> curves{};
  std::vector<VectorShape> shapes{};

  bool operator==(const VectorDrawing&) const = default;
};

// The point called id, or nullptr.
const VectorPoint* FindPoint(const VectorDrawing& drawing, PointId id);
VectorPoint* FindPoint(VectorDrawing* drawing, PointId id);
// Index of point id in drawing.points, or -1.
int PointIndex(const VectorDrawing& drawing, PointId id);

// True when every curve names points the drawing has, every shape names
// curves it has, and no limit is passed.
bool IsSound(const VectorDrawing& drawing);

}  // namespace snapper

#endif  // SNAPPER_MODEL_VECTOR_H_
