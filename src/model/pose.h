#ifndef SNAPPER_MODEL_POSE_H_
#define SNAPPER_MODEL_POSE_H_

#include <QPointF>

#include <vector>

namespace snapper {

// How far a piece (or a whole layer) is moved from rest. Angles are in
// degrees, clockwise on screen.
struct PiecePose final {
  double rotation = 0.0;
  QPointF offset;
  double scale_x = 1.0;
  double scale_y = 1.0;
  double skew = 0.0;
  double opacity = 1.0;
  // Which of the piece's drawings shows; -1 means the rig's default.
  int drawing = -1;
  // One offset per warp grid point, row by row; empty means no warp.
  std::vector<QPointF> warp;

  bool operator==(const PiecePose&) const = default;
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
