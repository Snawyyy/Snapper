#ifndef SNAPPER_ANIM_DOLL_POSE_H_
#define SNAPPER_ANIM_DOLL_POSE_H_

#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QTransform>

#include <map>
#include <vector>

#include "base/frame.h"
#include "model/doll.h"
#include "model/layer.h"
#include "model/pose.h"

namespace snapper {

// pose as a matrix that moves points around center: scale, skew, then
// rotate about center, then shift by the offset. Pieces, layers and IK
// all build their moves with this.
QTransform PoseMatrix(const PiecePose& pose, QPointF center);

// One piece ready to draw.
struct PlacedPiece final {
  QString name;
  // Full path of the drawing to show; empty when there is none.
  QString drawing;
  QSize size;
  // Drawing pixels to doll space.
  QTransform transform;
  int order = 0;
  double opacity = 1.0;
  WarpGrid grid;
  // Grid points in drawing pixels after the warp; empty when unwarped.
  std::vector<QPointF> warp_points;
  // Where the frame starts in the drawing shown (OwnDrawing::offset).
  QPoint offset;
};

using PoseMap = std::map<QString, PiecePose>;

// Every piece's pose at frame as keyed by hand; pieces without keys
// rest.
PoseMap SamplePoses(const DollLayer& layer, Frame frame);
// The same with doll's smart dials put under the hand poses and its worn
// skins swapping drawings (see ApplySkins): what the doll shows.
PoseMap SamplePoses(const Doll& doll, const DollLayer& layer, Frame frame);

// Drawing-to-doll-space matrix of every piece, parents first. Pieces
// whose parent is missing hang from nothing.
std::map<QString, QTransform> PieceTransforms(const Doll& doll,
                                              const PoseMap& poses);

// The layer's own place in shot space at frame, flip included.
QTransform LayerTransform(const Layer& layer, Frame frame);

// The whole doll posed, back to front.
std::vector<PlacedPiece> PlaceDoll(const Doll& doll, const PoseMap& poses);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DOLL_POSE_H_
