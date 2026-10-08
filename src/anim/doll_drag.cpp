#include "anim/doll_drag.h"

#include <QPointF>

#include <cassert>
#include <cmath>
#include <iterator>
#include <map>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "anim/warp.h"
#include "anim/warp_motion.h"

namespace snapper {
namespace {

// The spring of one drag node: how hard it is pulled to where its point
// is carried, and how much of its speed it keeps.
struct Spring final {
  QString piece;
  DragNode node;
  double pull = 0.0;
  double damping = 0.0;
  QPointF at;
  QPointF speed;
};

double Mix(double from, double to, double amount) {
  assert(std::isfinite(amount));
  assert(std::isfinite(from) && std::isfinite(to));
  return from + (to - from) * amount;
}

// Where a piece's grid point is carried at frame, in shot space, and
// the drawing-to-shot map it went through.
std::optional<std::pair<QPointF, QTransform>> Carried(
    const Doll& doll, const Layer& layer, const PoseMap& poses,
    const std::map<QString, QTransform>& placed, const Spring& spring,
    Frame frame) {
  assert(frame.index() >= 0);
  assert(!spring.piece.isEmpty());
  const auto transform = placed.find(spring.piece);
  const ArtPiece* art = FindArt(doll, spring.piece);
  const RigPiece* rig = FindRig(doll.rig, spring.piece);
  const bool is_placed =
      transform != placed.end() && art != nullptr && rig != nullptr;
  if (!is_placed) {
    return std::nullopt;
  }
  const auto pose = poses.find(spring.piece);
  const std::vector<QPointF> points = WarpedPoints(
      art->size, rig->warp,
      pose != poses.end() ? pose->second.warp : std::vector<QPointF>());
  const QTransform to_shot = transform->second * LayerTransform(layer, frame);
  return std::pair(
      to_shot.map(points[static_cast<size_t>(spring.node.point)]), to_shot);
}

using Carry = std::optional<std::pair<QPointF, QTransform>>;

// The doll's keys (its own move's and every piece's), each with whether
// the motion after it eases instead of holding.
std::map<Frame, bool> KeysOf(const Layer& layer) {
  assert(std::holds_alternative<DollLayer>(layer.content));
  std::map<Frame, bool> keys;
  const auto add = [&keys](const Channel<PiecePose>& channel) {
    for (const auto& key : channel.keys) {
      keys[key.frame] = keys[key.frame] || key.ease != Ease::kStep;
    }
  };
  add(layer.transform);
  const auto& pieces = std::get<DollLayer>(layer.content).pieces;
  for (const auto& [name, channel] : pieces) {
    add(channel);
  }
  return keys;
}

// The frame whose drag frame shows: drags move on the doll's own beat,
// so a doll on 3s wobbles on 3s. Each key is a beat, every frame of an
// ease is, and a long hold keeps beating at the spacing of the keys
// before it.
Frame BeatOf(const Layer& layer, Frame frame) {
  assert(frame.index() >= 0);
  const std::map<Frame, bool> keys = KeysOf(layer);
  const auto after = keys.upper_bound(frame);
  const bool has_key_before = after != keys.begin();
  if (!has_key_before) {
    return frame;
  }
  const auto at = std::prev(after);
  const bool is_easing = at->second && after != keys.end();
  if (is_easing) {
    return frame;
  }
  const bool has_earlier = at != keys.begin();
  const int spacing =
      has_earlier ? at->first.index() - std::prev(at)->first.index()
      : after != keys.end() ? after->first.index() - at->first.index()
                            : 1;
  const int since = frame.index() - at->first.index();
  assert(spacing >= 1);
  return Frame(at->first.index() + since / spacing * spacing);
}

// A spring per drag node of the doll.
std::vector<Spring> SpringsOf(const Doll& doll) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  std::vector<Spring> springs;
  for (const RigPiece& rig : doll.rig.pieces) {
    for (const DragNode& node : rig.drag_nodes) {
      const bool is_on_grid = node.point < rig.warp.PointCount();
      if (is_on_grid) {
        // More lag pulls softer; more bounce keeps more speed.
        const double pull = Mix(0.5, 0.04, node.lag);
        const double give = Mix(1.0, 0.08, node.bounce);
        springs.push_back({rig.name, node, pull,
                           2.0 * give * std::sqrt(pull), {}, {}});
      }
    }
  }
  assert(springs.size() <= static_cast<size_t>(kMaxDollPieces) *
                               static_cast<size_t>(kMaxWarpPoints));
  return springs;
}

// Runs the springs from the shot's first frame to frame; returns where
// each node's point is carried at frame.
std::vector<Carry> Run(const Doll& doll, const Layer& layer, Frame frame,
                       std::vector<Spring>* springs) {
  assert(springs != nullptr);
  assert(frame.index() >= 0);
  const auto& posed = std::get<DollLayer>(layer.content);
  std::vector<Carry> carried(springs->size());
  // ponytail: runs the springs from frame 0 on every call, so long shots
  // cost frames x pieces; cache states per frame if playback stutters.
  for (int f = 0; f <= frame.index(); ++f) {
    const PoseMap keyed = SamplePoses(posed, Frame(f));
    const auto placed = PieceTransforms(doll, keyed);
    for (size_t i = 0; i < springs->size(); ++i) {
      Spring& spring = (*springs)[i];
      carried[i] = Carried(doll, layer, keyed, placed, spring, Frame(f));
      const bool is_moving = carried[i].has_value() && f > 0;
      if (!is_moving) {
        spring.at = carried[i] ? carried[i]->first : spring.at;
        continue;
      }
      spring.speed += (carried[i]->first - spring.at) * spring.pull -
                      spring.speed * spring.damping;
      spring.at += spring.speed;
    }
  }
  return carried;
}

// Gives pose one warp offset per grid point (at rest when it had none
// or a stale count), ready to add to.
void FitWarp(int count, PiecePose* pose) {
  assert(pose != nullptr);
  assert(count >= 0 && count <= kMaxWarpPoints);
  const bool is_fitting = static_cast<int>(pose->warp.size()) == count;
  if (!is_fitting) {
    pose->warp.assign(static_cast<size_t>(count), QPointF());
  }
}

// Adds each piece's warp motion at beat to its warp.
void AddMotions(const Doll& doll, Frame beat, PoseMap* poses) {
  assert(poses != nullptr);
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  for (const RigPiece& rig : doll.rig.pieces) {
    const ArtPiece* art = FindArt(doll, rig.name);
    const bool is_moving =
        art != nullptr && rig.warp.IsOn() && rig.warp_motion.IsOn();
    if (!is_moving) {
      continue;
    }
    const std::vector<QPointF> pushes =
        MotionPushes(art->size, rig.warp, rig.warp_motion, beat);
    PiecePose& pose = (*poses)[rig.name];
    FitWarp(rig.warp.PointCount(), &pose);
    for (size_t point = 0; point < pushes.size(); ++point) {
      pose.warp[point] += pushes[point];
    }
  }
}

// Adds spring's trail (from where its point is carried) to the warp of
// its piece, spread by the point's rubber reach.
void AddTrail(const Doll& doll, const Spring& spring, const Carry& carried,
              PoseMap* poses) {
  assert(poses != nullptr);
  assert(!spring.piece.isEmpty());
  const RigPiece* rig = FindRig(doll.rig, spring.piece);
  bool is_invertible = false;
  const QTransform back =
      carried ? carried->second.inverted(&is_invertible) : QTransform();
  const bool is_usable = rig != nullptr && is_invertible;
  if (!is_usable) {
    return;
  }
  const QPointF trail = back.map(spring.at) - back.map(carried->first);
  PiecePose& pose = (*poses)[spring.piece];
  const int count = rig->warp.PointCount();
  FitWarp(count, &pose);
  const bool has_reach = static_cast<int>(rig->warp_reach.size()) == count;
  const double reach =
      has_reach ? rig->warp_reach[static_cast<size_t>(spring.node.point)]
                : 0.0;
  for (int other = 0; other < count; ++other) {
    pose.warp[static_cast<size_t>(other)] +=
        trail * PullWeight(rig->warp, spring.node.point, other, reach);
  }
}

}  // namespace

PoseMap DraggedPoses(const Doll& doll, const Layer& layer, Frame frame) {
  assert(frame.index() >= 0);
  assert(std::holds_alternative<DollLayer>(layer.content));
  PoseMap poses = SamplePoses(std::get<DollLayer>(layer.content), frame);
  const Frame beat = BeatOf(layer, frame);
  std::vector<Spring> springs = SpringsOf(doll);
  const bool has_springs = !springs.empty();
  if (has_springs) {
    const std::vector<Carry> carried = Run(doll, layer, beat, &springs);
    for (size_t i = 0; i < springs.size(); ++i) {
      AddTrail(doll, springs[i], carried[i], &poses);
    }
  }
  AddMotions(doll, beat, &poses);
  return poses;
}

}  // namespace snapper
