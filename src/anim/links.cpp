#include "anim/links.h"

#include <cassert>
#include <cmath>

#include "anim/layer_transform.h"

namespace snapper {

LinkSolver::LinkSolver(const Project& project, const Shot& shot)
    : project_(project), shot_(shot) {
  assert(shot.links.size() <= static_cast<size_t>(kMaxLinksPerShot));
  assert(project_.shots.size() <= static_cast<size_t>(kMaxShots));
}

QTransform LinkSolver::LayerTransform(const Layer& layer, Frame frame) {
  assert(frame.index() >= 0);
  assert(layer.id.value() >= 0);
  const QTransform base = snapper::LayerTransform(layer, frame);
  const Link* link = FindLink(shot_, LinkEnd{layer.id});
  if (link == nullptr) {
    return base;
  }
  const QPointF push = Push(*link, frame);
  return base * QTransform::fromTranslate(push.x(), push.y());
}

QPointF LinkSolver::Push(const Link& link, Frame frame) {
  assert(frame.index() >= 0);
  assert(link.strength >= 0.0);
  const std::pair<const Link*, int> key(&link, frame.index());
  const auto known = pushes_.find(key);
  if (known != pushes_.end()) {
    return known->second;
  }
  const bool is_loop = !solving_.insert(key).second;
  if (is_loop) {
    return QPointF();
  }
  const auto now = PointOf(link.leader, frame);
  const auto then = PointOf(link.leader, link.from);
  solving_.erase(key);
  const bool has_points = now.has_value() && then.has_value();
  const QPointF push =
      has_points ? (*now - *then) * link.strength : QPointF();
  const bool is_finite = std::isfinite(push.x()) && std::isfinite(push.y());
  return pushes_[key] = is_finite ? push : QPointF();
}

std::optional<QPointF> LinkSolver::PointOf(const LinkEnd& end, Frame frame) {
  assert(frame.index() >= 0);
  assert(end.layer.value() >= 0);
  const Layer* layer =
      end.layer.IsValid() ? FindLayer(shot_, end.layer) : nullptr;
  if (layer == nullptr) {
    return std::nullopt;
  }
  return LayerTransform(*layer, frame).map(QPointF());
}

bool WouldLoop(const Project& project, const Shot& shot,
               const LinkEnd& follower, const LinkEnd& leader) {
  assert(follower.layer.value() >= 0);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  // Walks back from the leader through whatever it follows.
  LinkEnd at = leader;
  for (int step = 0; step <= kMaxLinksPerShot; ++step) {
    if (at == follower) {
      return true;
    }
    const Link* link = FindLink(shot, at);
    if (link == nullptr) {
      return false;
    }
    at = link->leader;
  }
  return true;
}

}  // namespace snapper
