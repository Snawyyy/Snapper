#ifndef SNAPPER_ANIM_HOP_PREVIEW_H_
#define SNAPPER_ANIM_HOP_PREVIEW_H_

#include <QPointF>

#include <optional>
#include <vector>

#include "anim/cloth.h"
#include "anim/drag_spring.h"
#include "model/doll.h"

namespace snapper {

// One loop of the hop, in frames at 24 a second (about 1.2 s), and the
// frames of it spent in the air. Frame 0 is the landing.
inline constexpr int kHopFrames = 29;
inline constexpr int kHopAirFrames = 13;
// How high the body's bottom gets above the floor at the top of the
// hop, and the most it leans in the air (so Droop shows), in preview
// pixels and radians.
inline constexpr double kHopHeight = 10.0;
inline constexpr double kHopLean = 0.5;

// Where the body stands at frame of the loop: its bottom's height above
// the floor and its lean. Fixed: only the settings change what the body
// does about it.
struct HopPose final {
  double height = 0.0;
  double lean = 0.0;
};
HopPose HopAt(int frame);

// The live preview of a dot's settings: a small round blob that hops on
// a floor, lands, and reacts with the same solver the stage runs (jelly,
// cloth or the drag spring) on its edge dots, on a loop. Each hop starts
// from rest, so every loop looks the same. Still rests on the floor.
// Preview pixels: x from the middle, y down, the floor at 0.
class HopPreview final {
 public:
  // node is copied; nullptr (Still) rests.
  explicit HopPreview(const DragNode* node);

  // Shows node's settings from the next frame on, carrying on from
  // where the blob is when the kind stays (so dragging a feel does not
  // restart the hop), from rest when it changes. nullptr is Still.
  void SetNode(const DragNode* node);
  // Back to frame 0, at rest on the floor.
  void Rest();

  // One frame on; the loop never ends.
  void Step();

  int frame() const { return frame_; }
  // Every dot of the blob, and where the body carries each.
  const std::vector<QPointF>& dots() const { return dots_; }
  const std::vector<QPointF>& carried() const { return carried_; }
  // The edge dots in order round the blob (indices into dots()).
  static std::vector<int> Outline();
  // The dot a Drag preview hangs its spring on; -1 for the other kinds.
  int spring_dot() const;
  // How far the blob is off its carried shape: the farthest dot.
  double Bend() const;

 private:
  // was: where the body carried each dot the frame before.
  void StepPiece(const std::vector<QPointF>& was);
  void StepSpring();

  std::optional<DragNode> node_;
  WarpGrid grid_;
  ClothPiece piece_;
  DragSpring spring_;
  QPointF spring_at_;
  QPointF spring_speed_;
  int frame_ = 0;
  std::vector<QPointF> carried_;
  std::vector<QPointF> dots_;
};

}  // namespace snapper

#endif  // SNAPPER_ANIM_HOP_PREVIEW_H_
