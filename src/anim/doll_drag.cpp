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
#include "anim/doll_collide.h"
#include "anim/doll_colliders.h"
#include "anim/drag_spring.h"
#include "anim/grid_push.h"
#include "anim/jelly.h"
#include "anim/shot_grid.h"
#include "anim/warp.h"
#include "anim/warp_motion.h"

namespace snapper {
namespace {

// The spring of one drag node: how hard it is pulled to where its point
// is carried, and how much of its speed it keeps.
struct Spring final {
  QString piece;
  DragNode node;
  DragSpring spring;
  // A physics node moves with its piece's cloth, not as a lone spring.
  bool is_physics = false;
  QPointF at;
  QPointF speed;
};

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
        const bool is_physics = node.kind != NodeKind::kDrag;
        springs.push_back(
            {rig.name, node, DragSpringOf(node), is_physics, {}, {}});
      }
    }
  }
  assert(springs.size() <= static_cast<size_t>(kMaxDollPieces) *
                               static_cast<size_t>(kMaxWarpPoints));
  return springs;
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
  StepDragSpring(spring->spring, carry->first, &spring->at, &spring->speed);
}

// A piece's grid at a frame as the springs see it: its points carried
// there, pushed out of the colliders that push it, which it keeps for
// its cloth.
struct PushedGrid final {
  ShotGrid grid;
  std::vector<PushField> fields;
};

// The pushed grid of piece at f, or nothing off the stage.
std::optional<PushedGrid> PushedGridOf(const Doll& doll, const Layer& layer,
                                       const PoseMap& keyed,
                                       const std::map<QString, QTransform>&
                                           placed,
                                       const LinkShifts& shifts,
                                       const QString& piece, int f) {
  assert(f >= 0);
  assert(!piece.isEmpty());
  // A link only slides the piece, so the points move and the map back
  // to the drawing keeps its turn.
  const auto grid = ShotGridOf(doll, layer, keyed, placed, piece, Frame(f),
                               ShiftOf(shifts, piece, f));
  const RigPiece* rig = FindRig(doll.rig, piece);
  const bool is_placed = grid.has_value() && rig != nullptr;
  if (!is_placed) {
    return std::nullopt;
  }
  PushedGrid pushed{*grid, PushFieldsFor(doll, layer, keyed, placed, shifts,
                                         piece, Frame(f))};
  // Every piece is pushed from the same unpushed state here, so a piece
  // that is pushed back takes half.
  std::vector<bool> is_half;
  for (const PushField& field : pushed.fields) {
    is_half.push_back(field.is_shared);
  }
  PushGridShared(rig->warp, ReachesOf(*rig), pushed.fields, is_half,
                 &pushed.grid.points);
  return pushed;
}

// Runs the cloth or the jelly of one piece a frame: the springs at
// indices are its nodes of that kind, hung from pushed; was holds where
// the body carried each spring's point the frame before.
void StepPiece(const Doll& doll, const PushedGrid& pushed,
               const std::vector<size_t>& indices,
               const std::vector<Carry>& was, int f,
               std::vector<Spring>* springs) {
  assert(springs != nullptr);
  assert(!indices.empty() && indices.size() <= springs->size());
  const RigPiece* rig = FindRig(doll.rig, (*springs)[indices[0]].piece);
  assert(rig != nullptr);
  ClothPiece cloth;
  cloth.grid = rig->warp;
  cloth.fields = pushed.fields;
  cloth.drawn_down = DrawnDown(pushed.grid.to_shot);
  const ArtPiece* art = FindArt(doll, rig->name);
  cloth.cell = art != nullptr ? CellLength(rig->warp, art->size,
                                           pushed.grid.to_shot)
                              : 0.0;
  for (const size_t i : indices) {
    const Spring& spring = (*springs)[i];
    const QPointF now =
        pushed.grid.points[static_cast<size_t>(spring.node.point)];
    cloth.points.push_back(spring.node.point);
    cloth.settings.push_back(ClothSettingsOf(spring.node));
    cloth.at.push_back(spring.at);
    cloth.speed.push_back(spring.speed);
    cloth.body_speed.push_back(was[i] ? now - was[i]->first : QPointF());
  }
  const bool is_jelly = (*springs)[indices[0]].node.kind == NodeKind::kJelly;
  const bool is_first = f == 0;
  if (is_first) {
    RestCloth(pushed.grid.points, &cloth);
  } else if (is_jelly) {
    StepJelly(pushed.grid.points, &cloth);
  } else {
    StepCloth(pushed.grid.points, &cloth);
  }
  for (size_t k = 0; k < indices.size(); ++k) {
    (*springs)[indices[k]].at = cloth.at[k];
    (*springs)[indices[k]].speed = cloth.speed[k];
  }
}

// Runs one frame of the springs: where each node's point is carried at
// f (pushed out of colliders), the drag springs pulled toward it and
// each piece's cloth stepped.
void Step(const Doll& doll, const Layer& layer, int f,
          const LinkShifts& shifts, std::vector<Spring>* springs,
          std::vector<Carry>* carried) {
  assert(springs != nullptr && carried != nullptr);
  assert(f >= 0);
  const PoseMap keyed =
      SamplePoses(std::get<DollLayer>(layer.content), Frame(f));
  const auto placed = PieceTransforms(doll, keyed);
  std::map<QString, std::optional<PushedGrid>> grids;
  // Each piece's cloth nodes and its jelly nodes run apart.
  std::map<std::pair<QString, NodeKind>, std::vector<size_t>> cloths;
  const std::vector<Carry> was = *carried;
  for (size_t i = 0; i < springs->size(); ++i) {
    Spring& spring = (*springs)[i];
    const bool is_new_piece = grids.find(spring.piece) == grids.end();
    if (is_new_piece) {
      grids[spring.piece] =
          PushedGridOf(doll, layer, keyed, placed, shifts, spring.piece, f);
    }
    const std::optional<PushedGrid>& pushed = grids[spring.piece];
    Carry& carry = (*carried)[i];
    carry = std::nullopt;
    const bool is_on_stage = pushed.has_value();
    if (is_on_stage) {
      carry = std::pair(
          pushed->grid.points[static_cast<size_t>(spring.node.point)],
          pushed->grid.to_shot);
    }
    const bool is_cloth = spring.is_physics && is_on_stage;
    if (is_cloth) {
      cloths[{spring.piece, spring.node.kind}].push_back(i);
    } else {
      MoveSpring(carry, f, &spring);
    }
  }
  for (const auto& [group, indices] : cloths) {
    StepPiece(doll, *grids[group.first], indices, was, f, springs);
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

PoseMap DraggedPoses(const Doll& shared, const Layer& layer, Frame frame,
                     const LinkShifts& shifts, DragTrail* trail) {
  assert(frame.index() >= 0);
  assert(std::holds_alternative<DollLayer>(layer.content));
  // The layer's own drag nodes, motions and reach, where it has them.
  const DollFor own(shared, layer.id);
  const Doll& doll = own.get();
  PoseMap poses = SamplePoses(std::get<DollLayer>(layer.content), frame);
  const Frame beat = BeatOf(layer, frame);
  AddMotions(doll, beat, &poses);
  // Colliders push the drawn pose first; the springs ran from the same
  // pushed points, so their trails swing on top of the push.
  CollidePoses(doll, layer, frame, shifts, &poses);
  std::vector<Spring> springs = SpringsOf(doll);
  const bool has_springs = !springs.empty();
  if (has_springs) {
    const std::vector<Carry> carried =
        Run(doll, layer, beat, shifts, &springs, trail);
    for (size_t i = 0; i < springs.size(); ++i) {
      AddTrail(doll, springs[i], carried[i], &poses);
    }
  }
  return poses;
}

}  // namespace snapper
