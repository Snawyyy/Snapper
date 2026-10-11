#ifndef SNAPPER_ANIM_GUIDES_H_
#define SNAPPER_ANIM_GUIDES_H_

#include <QPointF>
#include <QRectF>

#include <vector>

namespace snapper {

// A line that something lined up with: vertical at x, or horizontal at
// y.
struct GuideLine final {
  bool is_vertical = false;
  double at = 0.0;

  bool operator==(const GuideLine&) const = default;
};

// How close, in screen pixels, an edge or middle must come to catch.
inline constexpr double kGuideReach = 6.0;

// What a move should add to line up, and the lines it lined up with.
struct GuideSnap final {
  QPointF shift;
  std::vector<GuideLine> lines;
};

// The lines moving already lines up with on targets (a mark within half
// a pixel): a drag leaves these alone, so it is not stuck where it
// began.
std::vector<GuideLine> AlignedGuides(const QRectF& moving,
                                     const std::vector<QRectF>& targets);

// Where moving (a box being dragged) catches on targets (boxes it may
// line up with: other things, the frame): its left, middle or right
// against each target's, and its top, middle or bottom against each
// target's, whichever pair is closest on an axis and within reach. Each
// axis catches on its own, and never on a line in skip. All in the same
// space.
GuideSnap SnapToGuides(const QRectF& moving,
                       const std::vector<QRectF>& targets, double reach,
                       const std::vector<GuideLine>& skip);

}  // namespace snapper

#endif  // SNAPPER_ANIM_GUIDES_H_
