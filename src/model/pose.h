#ifndef SNAPPER_MODEL_POSE_H_
#define SNAPPER_MODEL_POSE_H_

#include <QPointF>

namespace snapper {

// How far a layer is moved from rest: its own place on the stage, keyed.
// Angles are in degrees, clockwise on screen.
struct LayerPose final {
  double rotation = 0.0;
  QPointF offset{};
  double scale_x = 1.0;
  double scale_y = 1.0;
  double skew = 0.0;
  double opacity = 1.0;
  // Places in front of (above 0) or behind its place in the shot's stack.
  int order = 0;

  bool operator==(const LayerPose&) const = default;
};

// Where the shot's camera looks. center is the canvas point (from the
// canvas middle) shown in the middle of the frame.
struct CameraPose final {
  QPointF center;
  double zoom = 1.0;
  double rotation = 0.0;
  // Shake strength in pixels; the shake itself is seeded by the frame,
  // so it is the same in every render.
  double shake = 0.0;

  bool operator==(const CameraPose&) const = default;
};

}  // namespace snapper

#endif  // SNAPPER_MODEL_POSE_H_
