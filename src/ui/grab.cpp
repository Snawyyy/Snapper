#include "ui/grab.h"

#include <QLineF>

#include <algorithm>
#include <cassert>

namespace snapper {

Qt::CursorShape CursorFor(GrabKind kind) {
  assert(kind >= GrabKind::kNone && kind <= GrabKind::kEdge);
  assert(kGrabReach > kHandleDraw);
  switch (kind) {
    case GrabKind::kNone:
      return Qt::ArrowCursor;
    case GrabKind::kPivot:
      return Qt::SizeAllCursor;
    case GrabKind::kIkTip:
      return Qt::CrossCursor;
    case GrabKind::kLean:
      return Qt::SizeVerCursor;
    case GrabKind::kSwivel:
    case GrabKind::kKey:
    case GrabKind::kEdge:
      return Qt::SizeHorCursor;
    case GrabKind::kWarpDot:
      return Qt::PointingHandCursor;
  }
  return Qt::ArrowCursor;
}

int NearestGrab(const std::vector<QPointF>& spots, QPointF point,
                double reach) {
  assert(reach >= 0.0);
  assert(spots.size() < 1000000);
  int best = -1;
  double best_distance = reach;
  for (size_t i = 0; i < spots.size(); ++i) {
    const double distance = QLineF(spots[i], point).length();
    const bool is_in = distance <= reach;
    const bool is_closer = best < 0 || distance < best_distance;
    const bool is_better = is_in && is_closer;
    if (is_better) {
      best = static_cast<int>(i);
      best_distance = distance;
    }
  }
  return best;
}

double HoverAmount(double scale) {
  assert(kHoverGrow > 1.0);
  assert(scale >= 1.0);
  return std::clamp((scale - 1.0) / (kHoverGrow - 1.0), 0.0, 1.0);
}

bool IsPastDeadZone(QPointF press, QPointF now) {
  assert(kDragDeadZone > 0.0);
  assert(press.x() == press.x() && now.x() == now.x());
  return QLineF(press, now).length() >= kDragDeadZone;
}

void HandleGrace::Update(bool is_shown, int64_t now_ms) {
  assert(now_ms >= 0);
  assert(kGraceMs > 0);
  const bool is_hiding = was_shown_ && !is_shown;
  if (is_hiding) {
    hidden_at_ms_ = now_ms;
    has_hidden_ = true;
  }
  if (is_shown) {
    has_hidden_ = false;
  }
  was_shown_ = is_shown;
}

bool HandleGrace::IsGrabbable(bool is_shown, int64_t now_ms) const {
  assert(now_ms >= 0);
  assert(hidden_at_ms_ >= 0);
  const bool is_fresh = has_hidden_ && now_ms - hidden_at_ms_ <= kGraceMs;
  return is_shown || is_fresh;
}

}  // namespace snapper
