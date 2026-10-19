#include "anim/layer_transform.h"

#include <cassert>
#include <cmath>
#include <numbers>
#include <variant>

#include "anim/sampler.h"

namespace snapper {

QTransform PoseMatrix(const LayerPose& pose, QPointF center) {
  assert(std::isfinite(pose.rotation) && std::isfinite(pose.skew));
  assert(std::isfinite(center.x()) && std::isfinite(center.y()));
  const double skew = std::tan(pose.skew * std::numbers::pi / 180.0);
  QTransform shape;
  shape.scale(pose.scale_x, pose.scale_y);
  QTransform lean(1.0, 0.0, skew, 1.0, 0.0, 0.0);
  QTransform turn;
  turn.rotate(pose.rotation);
  return QTransform::fromTranslate(-center.x(), -center.y()) * shape * lean *
         turn *
         QTransform::fromTranslate(center.x() + pose.offset.x(),
                                   center.y() + pose.offset.y());
}

QTransform LayerTransform(const Layer& layer, Frame frame) {
  assert(frame.index() >= 0);
  assert(IsSorted(layer.transform));
  const LayerPose pose = Sample(layer.transform, frame, LayerPose());
  const auto* character = std::get_if<CharacterLayer>(&layer.content);
  const bool is_flipped = character != nullptr && character->is_flipped;
  const QTransform flip =
      is_flipped ? QTransform::fromScale(-1.0, 1.0) : QTransform();
  return flip * PoseMatrix(pose, QPointF());
}

}  // namespace snapper
