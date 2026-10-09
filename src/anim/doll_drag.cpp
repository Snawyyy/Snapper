#include "anim/doll_drag.h"

#include <QPointF>
#include <QTransform>

#include <algorithm>
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

// Lag below this eases into no trail at all.
constexpr double kSnapLag = 0.1;

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

using Carry = DragTrail::Carry;

// The most frames a trail keeps; a longer shot runs on without it.
constexpr size_t kMaxTrailFrames = 20000;

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
  assert(keys.size() <= layer.transform.keys.size() +
                            pieces.size() * kMaxTrailFrames);
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
        const double soft_pull = Mix(0.5, 0.04, node.lag);
        const double give = Mix(1.0, 0.08, node.bounce);
        const double soft_damping = 2.0 * give * std::sqrt(soft_pull);
        // Below kSnapLag the spring blends into one with pull and
        // damping 1, which lands on its point every frame, so lag 0
        // trails nothing and the slider has no jump near 0.
        const double snap =
            std::clamp(1.0 - node.lag / kSnapLag, 0.0, 1.0);
        springs.push_back({rig.name, node, Mix(soft_pull, 1.0, snap),
                           Mix(soft_damping, 1.0, snap), {}, {}});
      }
    }
  }
  assert(springs.size() <= static_cast<size_t>(kMaxDollPieces) *
                               static_cast<size_t>(kMaxWarpPoints));
  return springs;
}

// How far links carry piece at frame, in shot space.
QPointF ShiftOf(const LinkShifts& shifts, const QString& piece, int frame) {
  assert(frame >= 0);
  assert(!piece.isEmpty());
  const auto found = shifts.find(piece);
  const bool is_shifted = found != shifts.end() &&
                          static_cast<size_t>(frame) < found->second.size();
  return is_shifted ? found->second[static_cast<size_t>(frame)] : QPointF();
}

// Puts the springs back as the trail left them after its frame.
void Restore(const DragTrail::State& state, std::vector<Spring>* springs,
             std::vector<Carry>* carried) {
  assert(springs != nullptr && carried != nullptr);
  assert(state.at.size() == springs->size());
  for (size_t i = 0; i < springs->size(); ++i) {
    (*springs)[i].at = state.at[i];
    (*springs)[i].speed = state.speed[i];
  }
  *carried = state.carried;
}

DragTrail::State Snapshot(const std::vector<Spring>& springs,
                          const std::vector<Carry>& carried) {
  assert(springs.size() == carried.size());
  assert(springs.size() <= static_cast<size_t>(kMaxDollPieces) *
                               static_cast<size_t>(kMaxWarpPoints));
  DragTrail::State state;
  for (const Spring& spring : springs) {
    state.at.push_back(spring.at);
    state.speed.push_back(spring.speed);
  }
  state.carried = carried;
  return state;
}

// Runs one frame of the springs: where each node's point is carried at
// f, and the spring pulled toward it.
void Step(const Doll& doll, const Layer& layer, int f,
          const LinkShifts& shifts, std::vector<Spring>* springs,
          std::vector<Carry>* carried) {
  assert(springs != nullptr && carried != nullptr);
  assert(f >= 0);
  const auto& posed = std::get<DollLayer>(layer.content);
  const PoseMap keyed = SamplePoses(posed, Frame(f));
  const auto placed = PieceTransforms(doll, keyed);
  for (size_t i = 0; i < springs->size(); ++i) {
    Spring& spring = (*springs)[i];
    Carry& carry = (*carried)[i];
    carry = Carried(doll, layer, keyed, placed, spring, Frame(f));
    // A link only slides the piece, so the point moves and the map
    // back to the drawing keeps its turn.
    const bool is_carried = carry.has_value();
    if (is_carried) {
      const QPointF shift = ShiftOf(shifts, spring.piece, f);
      carry->first += shift;
      carry->second *= QTransform::fromTranslate(shift.x(), shift.y());
    }
    const bool is_moving = is_carried && f > 0;
    if (!is_moving) {
      spring.at = is_carried ? carry->first : spring.at;
      continue;
    }
    spring.speed += (carry->first - spring.at) * spring.pull -
                    spring.speed * spring.damping;
    spring.at += spring.speed;
  }
}

// Runs the springs from the shot's first frame to frame (or from where
// the trail left off); returns where each node's point is carried at
// frame.
std::vector<Carry> Run(const Doll& doll, const Layer& layer, Frame frame,
                       const LinkShifts& shifts,
                       std::vector<Spring>* springs, DragTrail* trail) {
  assert(springs != nullptr);
  assert(frame.index() >= 0);
  std::vector<Carry> carried(springs->size());
  int first = 0;
  const bool has_trail = trail != nullptr;
  const bool is_stale =
      has_trail && !trail->frames().empty() &&
      trail->frames().front().at.size() != springs->size();
  if (is_stale) {
    trail->Clear();
  }
  const size_t kept = has_trail ? trail->frames().size() : 0;
  const bool is_kept = kept > 0;
  if (is_kept) {
    const size_t at = std::min(kept - 1, static_cast<size_t>(frame.index()));
    Restore(trail->frames()[at], springs, &carried);
    first = static_cast<int>(at) + 1;
  }
  for (int f = first; f <= frame.index(); ++f) {
    Step(doll, layer, f, shifts, springs, &carried);
    const bool is_next = has_trail && trail->frames().size() ==
                                          static_cast<size_t>(f);
    if (is_next) {
      trail->Add(Snapshot(*springs, carried));
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

// The rubber reach of rig's point, 0 when it has none.
double ReachOf(const RigPiece& rig, int point) {
  assert(point >= 0 && point < rig.warp.PointCount());
  assert(rig.warp_reach.size() <= static_cast<size_t>(kMaxWarpPoints));
  const bool has_reach =
      static_cast<int>(rig.warp_reach.size()) == rig.warp.PointCount();
  return has_reach ? rig.warp_reach[static_cast<size_t>(point)] : 0.0;
}

// Adds each point motion at beat to the warp of its piece, spread by
// the point's rubber reach.
void AddMotions(const Doll& doll, Frame beat, PoseMap* poses) {
  assert(poses != nullptr);
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  for (const RigPiece& rig : doll.rig.pieces) {
    const int count = rig.warp.PointCount();
    for (const PointMotion& motion : rig.point_motions) {
      const bool is_on_grid = motion.point >= 0 && motion.point < count;
      if (!is_on_grid) {
        continue;
      }
      const std::vector<QPointF> pushes = PointMotionPushes(
          rig.warp, motion, ReachOf(rig, motion.point), beat);
      PiecePose& pose = (*poses)[rig.name];
      FitWarp(count, &pose);
      for (size_t point = 0; point < pushes.size(); ++point) {
        pose.warp[point] += pushes[point];
      }
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
  const double reach = ReachOf(*rig, spring.node.point);
  for (int other = 0; other < count; ++other) {
    pose.warp[static_cast<size_t>(other)] +=
        trail * PullWeight(rig->warp, spring.node.point, other, reach);
  }
}

}  // namespace

void DragTrail::Add(State state) {
  assert(frames_.size() <= kMaxTrailFrames);
  assert(frames_.empty() || frames_.front().at.size() == state.at.size());
  const bool is_room = frames_.size() < kMaxTrailFrames;
  if (is_room) {
    frames_.push_back(std::move(state));
  }
}

PoseMap DraggedPoses(const Doll& doll, const Layer& layer, Frame frame,
                     const LinkShifts& shifts, DragTrail* trail) {
  assert(frame.index() >= 0);
  assert(std::holds_alternative<DollLayer>(layer.content));
  PoseMap poses = SamplePoses(std::get<DollLayer>(layer.content), frame);
  const Frame beat = BeatOf(layer, frame);
  std::vector<Spring> springs = SpringsOf(doll);
  const bool has_springs = !springs.empty();
  if (has_springs) {
    const std::vector<Carry> carried =
        Run(doll, layer, beat, shifts, &springs, trail);
    for (size_t i = 0; i < springs.size(); ++i) {
      AddTrail(doll, springs[i], carried[i], &poses);
    }
  }
  AddMotions(doll, beat, &poses);
  return poses;
}

}  // namespace snapper
