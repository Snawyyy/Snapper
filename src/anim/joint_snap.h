#ifndef SNAPPER_ANIM_JOINT_SNAP_H_
#define SNAPPER_ANIM_JOINT_SNAP_H_

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QSize>
#include <QTransform>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace snapper {

// One solid shape of a drawing: where its middle is, which way it is
// long, and how far it reaches along and across that way.
struct LimbShape final {
  QPointF center;
  // Unit length, along the shape's long direction.
  QPointF along;
  // How far the shape reaches from center along `along` (low < 0 <
  // high) and, half, across it.
  double low = 0.0;
  double high = 0.0;
  double half_width = 0.0;
  int pixels = 0;
};

// The shapes of a drawing, measured once: every pixel a viewer could
// see (alpha kMeshAlpha and up, the shape mesh's rule), with holes
// closed in, so a limb drawn as an outline only counts as filled.
// Separate shapes get separate labels.
struct LimbMask final {
  QSize size;
  // Per pixel, row by row: 0 for empty, else the shape's index plus 1.
  std::vector<std::int32_t> labels;
  std::vector<LimbShape> shapes;

  // The label of the pixel at (drawing pixels); 0 outside.
  int LabelAt(QPointF at) const;
};

LimbMask MeasureLimbs(const QImage& drawing);

enum class ShapeSnapKind { kWidthMiddle, kEndCenter, kOverlapCenter };

// A spot a joint can catch on, in the drawing's pixels, and what to
// draw for it: the line straight across the limb there, edge to edge,
// and for an end, the end circle's radius (0 otherwise).
struct ShapeSnap final {
  ShapeSnapKind kind = ShapeSnapKind::kWidthMiddle;
  QPointF spot;
  QPointF edge_a;
  QPointF edge_b;
  double radius = 0.0;
};

// The middle of the limb's width straight across its long direction
// through at; none when at is on no shape.
std::optional<ShapeSnap> WidthMiddleAt(const LimbMask& mask, QPointF at);

// The center of each end circle of each shape: half a width in from
// the tip, on the middle of the width there.
std::vector<ShapeSnap> EndCentersOf(const LimbMask& mask);

// Where the center lines of own's and parent's biggest shapes cross,
// when that is inside both. parent_to_own maps parent's drawing pixels
// into own's.
std::optional<ShapeSnap> OverlapCenter(const LimbMask& own,
                                       const LimbMask& parent,
                                       const QTransform& parent_to_own);

// What a dragged pivot catches on, and where it lands.
struct PivotSnap final {
  QPointF spot;
  // The shape snap it caught; none for a box snap or none at all.
  std::optional<ShapeSnap> shape;
  bool is_snapped = false;
};

// wanted (a pivot in own's drawing pixels) caught within reach (the
// same pixels) on own's shape or its box. A spot (end center, overlap
// center) wins first; else the nearer of the width middle and the box's
// middle and edges. parent may be null.
PivotSnap SnapPivot(const LimbMask& own, const LimbMask* parent,
                    const QTransform& parent_to_own, QPointF wanted,
                    double reach);

// Where Snap to shape moves a pivot at: the end or overlap center whose
// circle it is in, else the width middle through it; none when it is on
// no shape.
std::optional<ShapeSnap> NearestShapeSnap(const LimbMask& own,
                                          const LimbMask* parent,
                                          const QTransform& parent_to_own,
                                          QPointF at);

// The radius of a joint circle at pivot: half the limb's width straight
// across it there; 0 when pivot is on no shape.
double JointRadius(const LimbMask& mask, QPointF pivot);

// The most common solid color of drawing within a few pixels of at, for
// a new joint circle; black when none is there.
QColor JointColorAt(const QImage& drawing, QPointF at);

// Measured drawings kept by the image's cache key, so a drag measures
// each drawing once. What Of gives stays good after later calls.
class LimbMemo final {
 public:
  std::shared_ptr<const LimbMask> Of(const QImage& drawing);

 private:
  std::map<std::int64_t, std::shared_ptr<const LimbMask>> masks_;
};

}  // namespace snapper

#endif  // SNAPPER_ANIM_JOINT_SNAP_H_
