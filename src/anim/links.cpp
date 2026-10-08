#include "anim/links.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>
#include <variant>

#include "anim/doll_lean.h"

namespace snapper {
namespace {

const Doll* DollOf(const Project& project, const Layer& layer) {
  assert(layer.id.value() >= 0);
  const auto* posed = std::get_if<DollLayer>(&layer.content);
  assert(project.dolls.size() <= static_cast<size_t>(kMaxProjectDolls));
  return posed != nullptr ? FindDoll(project, posed->doll) : nullptr;
}

// m without its shift: what it does to a direction.
QTransform Turn(const QTransform& m) {
  assert(std::isfinite(m.m11()) && std::isfinite(m.m22()));
  assert(std::isfinite(m.m12()) && std::isfinite(m.m21()));
  return QTransform(m.m11(), m.m12(), m.m21(), m.m22(), 0.0, 0.0);
}

}  // namespace

LinkSolver::LinkSolver(const Project& project, const Shot& shot)
    : project_(project), shot_(shot) {
  assert(shot_.links.size() <= static_cast<size_t>(kMaxLinksPerShot));
  assert(shot_.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
}

const PoseMap& LinkSolver::BasePoses(const Doll& doll, const Layer& layer,
                                     Frame frame) {
  assert(frame.index() >= 0);
  assert(layer.id.value() >= 0);
  const std::pair<int, int> key(layer.id.value(), frame.index());
  const auto found = poses_.find(key);
  const bool is_known = found != poses_.end();
  if (is_known) {
    return found->second;
  }
  return poses_[key] = ShownPoses(doll, layer, frame);
}

QTransform LinkSolver::LayerTransform(const Layer& layer, Frame frame) {
  assert(frame.index() >= 0);
  assert(layer.id.value() >= 0);
  const QTransform base = snapper::LayerTransform(layer, frame);
  const Link* link = FindLink(shot_, LinkEnd{layer.id, QString()});
  const bool is_linked = link != nullptr;
  if (!is_linked) {
    return base;
  }
  const QPointF push = Push(*link, frame);
  return base * QTransform::fromTranslate(push.x(), push.y());
}

PoseMap LinkSolver::Poses(const Doll& doll, const Layer& layer,
                          Frame frame) {
  assert(frame.index() >= 0);
  assert(std::holds_alternative<DollLayer>(layer.content));
  PoseMap poses = BasePoses(doll, layer, frame);
  const QTransform to_shot = snapper::LayerTransform(layer, frame);
  std::map<QString, QTransform> placed;
  for (const Link& link : shot_.links) {
    const bool is_ours =
        link.follower.layer == layer.id && !link.follower.piece.isEmpty();
    const RigPiece* rig =
        is_ours ? FindRig(doll.rig, link.follower.piece) : nullptr;
    const bool is_followed_piece = rig != nullptr;
    if (!is_followed_piece) {
      continue;
    }
    // Turns only, so the links' own shifts never change them.
    const bool is_unplaced = placed.empty();
    if (is_unplaced) {
      placed = PieceTransforms(doll, BasePoses(doll, layer, frame));
    }
    const auto parent = placed.find(rig->parent);
    const bool has_parent =
        !rig->parent.isEmpty() && parent != placed.end();
    const QTransform frame_of = has_parent ? parent->second * to_shot
                                           : to_shot;
    bool is_invertible = false;
    const QTransform back = Turn(frame_of).inverted(&is_invertible);
    if (!is_invertible) {
      continue;
    }
    poses[link.follower.piece].offset += back.map(Push(link, frame));
  }
  return poses;
}

QPointF LinkSolver::Push(const Link& link, Frame frame) {
  assert(frame.index() >= 0);
  assert(link.strength >= 0.0);
  const std::pair<const Link*, int> key(&link, frame.index());
  const auto known = pushes_.find(key);
  const bool is_known = known != pushes_.end();
  if (is_known) {
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

std::optional<QPointF> LinkSolver::PointOf(const LinkEnd& end,
                                           Frame frame) {
  assert(frame.index() >= 0);
  assert(end.layer.value() >= 0);
  const Layer* layer =
      end.layer.IsValid() ? FindLayer(shot_, end.layer) : nullptr;
  const bool has_layer = layer != nullptr;
  if (!has_layer) {
    return std::nullopt;
  }
  const QTransform to_shot = snapper::LayerTransform(*layer, frame);
  const bool is_whole = end.piece.isEmpty();
  if (is_whole) {
    return to_shot.map(QPointF()) + Carried(end, frame);
  }
  const Doll* doll = DollOf(project_, *layer);
  const RigPiece* rig =
      doll != nullptr ? FindRig(doll->rig, end.piece) : nullptr;
  const bool has_rig = rig != nullptr;
  if (!has_rig) {
    return std::nullopt;
  }
  const auto placed = PieceTransforms(*doll, BasePoses(*doll, *layer, frame));
  const auto piece = placed.find(end.piece);
  const bool is_placed = piece != placed.end();
  if (!is_placed) {
    return std::nullopt;
  }
  return to_shot.map(piece->second.map(rig->pivot)) +
         Carried(end, frame);
}

QPointF LinkSolver::Carried(const LinkEnd& end, Frame frame) {
  assert(frame.index() >= 0);
  assert(end.layer.value() >= 0);
  QPointF moved;
  for (const LinkEnd& carrier : CarriersOf(project_, shot_, end)) {
    const Link* link = FindLink(shot_, carrier);
    const bool is_carried = link != nullptr;
    if (is_carried) {
      moved += Push(*link, frame);
    }
  }
  return moved;
}

std::vector<LinkEnd> CarriersOf(const Project& project, const Shot& shot,
                                const LinkEnd& end) {
  assert(end.layer.value() >= 0);
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  std::vector<LinkEnd> carriers = {end};
  const bool is_whole = end.piece.isEmpty();
  if (is_whole) {
    return carriers;
  }
  const Layer* layer =
      end.layer.IsValid() ? FindLayer(shot, end.layer) : nullptr;
  const Doll* doll = layer != nullptr ? DollOf(project, *layer) : nullptr;
  const RigPiece* rig =
      doll != nullptr ? FindRig(doll->rig, end.piece) : nullptr;
  // Up the parents, bounded in case a damaged rig loops.
  for (int i = 0; rig != nullptr && i < kMaxDollPieces; ++i) {
    const bool has_parent = !rig->parent.isEmpty();
    rig = has_parent ? FindRig(doll->rig, rig->parent) : nullptr;
    const bool has_rig = rig != nullptr;
    if (has_rig) {
      carriers.push_back(LinkEnd{end.layer, rig->name});
    }
  }
  carriers.push_back(LinkEnd{end.layer, QString()});
  return carriers;
}

bool WouldLoop(const Project& project, const Shot& shot,
               const LinkEnd& follower, const LinkEnd& leader) {
  assert(follower.layer.value() >= 0);
  assert(leader.layer.value() >= 0);
  // Walks back from the leader through everything that moves it.
  std::vector<LinkEnd> waiting = {leader};
  std::set<LinkEnd> seen;
  for (int step = 0; !waiting.empty() && step <= kMaxLinksPerShot * 2;
       ++step) {
    const LinkEnd at = waiting.back();
    waiting.pop_back();
    const bool is_new = seen.insert(at).second;
    if (!is_new) {
      continue;
    }
    for (const LinkEnd& carrier : CarriersOf(project, shot, at)) {
      const bool is_follower = carrier == follower;
      if (is_follower) {
        return true;
      }
      const Link* link = FindLink(shot, carrier);
      const bool is_carried = link != nullptr;
      if (is_carried) {
        waiting.push_back(link->leader);
      }
    }
  }
  return false;
}

}  // namespace snapper
