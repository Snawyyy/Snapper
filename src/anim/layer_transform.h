#ifndef SNAPPER_ANIM_LAYER_TRANSFORM_H_
#define SNAPPER_ANIM_LAYER_TRANSFORM_H_

#include <QPointF>
#include <QTransform>

#include "base/frame.h"
#include "model/layer.h"
#include "model/pose.h"

namespace snapper {

// pose as a matrix around center: scale, skew, turn, then the move.
QTransform PoseMatrix(const LayerPose& pose, QPointF center);

// The layer's own place in shot space at frame, flip included.
QTransform LayerTransform(const Layer& layer, Frame frame);

}  // namespace snapper

#endif  // SNAPPER_ANIM_LAYER_TRANSFORM_H_
