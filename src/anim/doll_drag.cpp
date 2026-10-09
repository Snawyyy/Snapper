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

#include "anim/cloth.h"
#include "anim/doll_beat.h"
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
  // A physics node moves with its piece's cloth, not as a lone spring.
  bool is_physics = false;
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

// Every point of a piece's grid as it is carried at frame, in shot
// space, and the drawing-to-shot map it went through.
struct ShotGrid final {
  std::vector<QPointF> points;
  QTransform to_shot;
};

std::optional<ShotGrid> ShotGridOf(
    const Doll& doll, const Layer& layer, const PoseMap& poses,
    const std::map<QString, QTransform>& placed, const QString& piece,
    Frame frame) {
  assert(frame.index() >= 0);
  assert(!piece.isEmpty());
  const auto transform = placed.find(piece);
  const ArtPiece* art = FindArt(doll, piece);
  const RigPiece* rig = FindRig(doll.rig, piece);
  const bool is_placed =
      transform != placed.end() && art != nullptr && rig != nullptr;
  if (!is_placed) {
    return std::nullopt;
  }
  const auto pose = poses.find(piece);
  ShotGrid grid;
  grid.points = WarpedPoints(
      art->size, rig->warp,
      pose != poses.end() ? pose->second.warp : std::vector<QPointF>());
  grid.to_shot = transform->second * LayerTransform(layer, frame);
  for (QPointF& point : grid.points) {
    point = grid.to_shot.map(point);
  }
  return grid;
}

using Carry = DragTrail::Carry;

// The most frames a trail keeps; a longer shot runs on without it.
constexpr size_t kMaxTrailFrames = 20000;

// A spring per drag node of the doll.
std::vector<Spring> SpringsOf(const Doll& doll) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  std::vector<Spring> springs;
  for (const RigPiece& rig : doll.rig.pieces) {
    for (const DragNode& node : rig.drag_nodes) {
      const bool is_on_grid = node.point < rig.warp.PointCount();
      if (is_on_grid) {
        const bool is_physics = node.physics;
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
                           Mix(soft_damping, 1.0, snap), is_physics, {}, {}});
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

// Moves a lone drag spring one frame toward where its point is carried.
void MoveSpring(const Carry& carry, int f, Spring* spring) {
  assert(spring != nullptr);
  assert(f >= 0);
  const bool is_moving = carry.has_value() && f > 0;
  if (!is_moving) {
    spring->at = carry.has_value() ? carry->first : spring->at;
    return;
  }
  spring->speed += (carry->first - spring->at) * spring->pull -
                   spring->speed * spring->damping;
  spring->at += spring->speed;
}

// Runs the cloth of one piece a frame: the springs at indices are its
// physics nodes, carried over grid.
void StepPiece(const Doll& doll, const ShotGrid& grid,
               const std::vector<size_t>& indices, int f,
               std::vector<Spring>* springs) {
  assert(springs != nullptr);
  assert(!indices.empty() && indices.size() <= springs->size());
  const RigPiece* rig = FindRig(doll.rig, (*springs)[indices[0]].piece);
  assert(rig != nullptr);
  ClothPiece cloth;
  cloth.grid = rig->warp;
  for (const size_t i : indices) {
    const Spring& spring = (*springs)[i];
    cloth.points.push_back(spring.node.point);
    cloth.settings.push_back(ClothSettingsOf(spring.node));
    cloth.at.push_back(spring.at);
    cloth.speed.push_back(spring.speed);
  }
  const bool is_first = f == 0;
  if (is_first) {
    RestCloth(grid.points, &cloth);
  } else {
    StepCloth(grid.points, &cloth);
  }
  for (size_t k = 0; k < indices.size(); ++k) {
    (*springs)[indices[k]].at = cloth.at[k];
    (*springs)[indices[k]].speed = cloth.speed[k];
  }
}

// Runs one frame of the springs: where each node's point is carried at
// f, the drag springs pulled toward it and each piece's cloth stepped.
void Step(const Doll& doll, const Layer& layer, int f,
          const LinkShifts& shifts, std::vector<Spring>* springs,
          std::vector<Carry>* carried) {
  assert(springs != nullptr && carried != nullptr);
  assert(f >= 0);
  const auto& posed = std::get<DollLayer>(layer.content);
  const PoseMap keyed = SamplePoses(posed, Frame(f));
  const auto placed = PieceTransforms(doll, keyed);
  std::map<QString, std::optional<ShotGrid>> grids;
  std::map<QString, std::vector<size_t>> cloths;
  for (size_t i = 0; i < springs->size(); ++i) {
    Spring& spring = (*springs)[i];
    const bool is_new_piece = grids.find(spring.piece) == grids.end();
    if (is_new_piece) {
      grids[spring.piece] = ShotGridOf(doll, layer, keyed, placed,
                                       spring.piece, Frame(f));
    }
    const std::optional<ShotGrid>& grid = grids[spring.piece];
    // A link only slides the piece, so the point moves and the map
    // back to the drawing keeps its turn.
    const QPointF shift = ShiftOf(shifts, spring.piece, f);
    Carry& carry = (*carried)[i];
    carry = std::nullopt;
    const bool is_on_stage = grid.has_value();
    if (is_on_stage) {
      carry = std::pair(
          grid->points[static_cast<size_t>(spring.node.point)] + shift,
          grid->to_shot * QTransform::fromTranslate(shift.x(), shift.y()));
    }
    const bool is_cloth = spring.is_physics && is_on_stage;
    if (is_cloth) {
      cloths[spring.piece].push_back(i);
    } else {
      MoveSpring(carry, f, &spring);
    }
  }
  for (const auto& [piece, indices] : cloths) {
    ShotGrid shifted = *grids[piece];
    const QPointF shift = ShiftOf(shifts, piece, f);
    for (QPointF& point : shifted.points) {
      point += shift;
    }
    StepPiece(doll, shifted, indices, f, springs);
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
