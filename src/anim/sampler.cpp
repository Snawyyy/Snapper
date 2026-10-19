#include "anim/sampler.h"

#include <algorithm>
#include <cassert>
#include <cmath>


namespace snapper {

double EaseAmount(Ease ease, double t) {
  assert(std::isfinite(t));
  assert(kEaseCount == 5);
  const double x = std::clamp(t, 0.0, 1.0);
  switch (ease) {
    case Ease::kStep:
      return 0.0;
    case Ease::kLinear:
      return x;
    case Ease::kEaseIn:
      return x * x;
    case Ease::kEaseOut:
      return 1.0 - (1.0 - x) * (1.0 - x);
    case Ease::kEaseInOut:
      return x * x * (3.0 - 2.0 * x);
  }
  return x;
}

double Lerp(double a, double b, double t) {
  assert(std::isfinite(a) && std::isfinite(b));
  assert(std::isfinite(t));
  return a + (b - a) * t;
}

QPointF Lerp(QPointF a, QPointF b, double t) {
  assert(std::isfinite(a.x()) && std::isfinite(a.y()));
  assert(std::isfinite(t));
  return a + (b - a) * t;
}

LayerPose Lerp(const LayerPose& a, const LayerPose& b, double t) {
  assert(std::isfinite(t));
  assert(std::isfinite(a.rotation) && std::isfinite(b.rotation));
  LayerPose out = a;
  out.rotation = Lerp(a.rotation, b.rotation, t);
  out.offset = Lerp(a.offset, b.offset, t);
  out.scale_x = Lerp(a.scale_x, b.scale_x, t);
  out.scale_y = Lerp(a.scale_y, b.scale_y, t);
  out.skew = Lerp(a.skew, b.skew, t);
  out.opacity = Lerp(a.opacity, b.opacity, t);
  return out;
}

CameraPose Lerp(const CameraPose& a, const CameraPose& b, double t) {
  assert(std::isfinite(t));
  assert(std::isfinite(a.zoom) && std::isfinite(b.zoom));
  CameraPose out;
  out.center = Lerp(a.center, b.center, t);
  out.zoom = Lerp(a.zoom, b.zoom, t);
  out.rotation = Lerp(a.rotation, b.rotation, t);
  out.shake = Lerp(a.shake, b.shake, t);
  return out;
}

}  // namespace snapper
