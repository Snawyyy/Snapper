#ifndef SNAPPER_EDIT_RIG_EDITS_H_
#define SNAPPER_EDIT_RIG_EDITS_H_

// RigManager's building blocks, shared by its source files.

#include <QString>

#include <algorithm>
#include <cassert>
#include <memory>
#include <variant>
#include <vector>

#include "anim/warp.h"
#include "base/error.h"
#include "base/text.h"
#include "edit/rig_manager.h"
#include "model/project.h"

namespace snapper {

inline Error NoPiece(const QString& doll, const QString& piece) {
  assert(!doll.isEmpty());
  assert(!piece.isEmpty());
  return Error{Tr("%1 has no piece called %2.").arg(doll, piece)};
}

inline Error NoChain(const QString& doll, const QString& chain) {
  assert(!doll.isEmpty());
  assert(!chain.isEmpty());
  return Error{Tr("%1 has no IK chain called %2.").arg(doll, chain)};
}

// The project with doll's rig changed by change, which may refuse.
template <typename Change>
Result<Project> WithRig(const Project& project, const QString& doll,
                        Change change) {
  assert(!doll.isEmpty());
  assert(project.dolls.size() <= static_cast<size_t>(kMaxProjectDolls));
  const Doll* found = FindDoll(project, doll);
  const bool is_present = found != nullptr;
  if (!is_present) {
    return std::unexpected(Error{Tr("%1 isn't in the project.").arg(doll)});
  }
  Doll changed = *found;
  const Result<void> done = change(&changed.rig);
  if (!done) {
    return std::unexpected(done.error());
  }
  Project next = project;
  next.dolls[doll] = std::make_shared<const Doll>(std::move(changed));
  return next;
}

// motion with every number kept in range.
inline PointMotion KeptMotion(PointMotion motion) {
  assert(std::isfinite(motion.size) && std::isfinite(motion.angle));
  assert(std::isfinite(motion.delay));
  motion.size = std::clamp(motion.size, 0.0, kMaxWarpMotionSize);
  motion.cycle = std::clamp(motion.cycle, kMinWarpCycle, kMaxWarpCycle);
  motion.angle = std::remainder(motion.angle, 360.0);
  motion.delay = std::clamp(motion.delay, 0.0, 1.0);
  return motion;
}

// The middle of a piece's drawing in doll space.
inline double ArtCentreX(const ArtPiece& art) {
  assert(art.size.width() >= 0);
  assert(std::isfinite(art.position.x()));
  return art.position.x() + art.size.width() / 2.0;
}

// motion as it is on the other side: on point (the mirrored one), left
// and right swapped, up and down staying.
inline PointMotion MirroredMotion(PointMotion motion, int point) {
  assert(point >= 0 && point < kMaxWarpPoints);
  assert(std::isfinite(motion.angle));
  motion.point = point;
  motion.angle = std::remainder(180.0 - motion.angle, 360.0);
  return motion;
}

// The same, for one piece of the rig.
template <typename Change>
Result<Project> WithPiece(const Project& project, const QString& doll,
                          const QString& piece, Change change) {
  assert(!piece.isEmpty());
  assert(!doll.isEmpty());
  return WithRig(project, doll, [&](Rig* rig) -> Result<void> {
    RigPiece* found = FindRig(rig, piece);
    const bool is_present = found != nullptr;
    if (!is_present) {
      return std::unexpected(NoPiece(doll, piece));
    }
    return change(found);
  });
}

// The same, for one IK chain.
template <typename Change>
Result<Project> WithChain(const Project& project, const QString& doll,
                          const QString& chain, Change change) {
  assert(!chain.isEmpty());
  assert(!doll.isEmpty());
  return WithRig(project, doll, [&](Rig* rig) -> Result<void> {
    const auto found =
        std::find_if(rig->chains.begin(), rig->chains.end(),
                     [&chain](const IkChain& c) { return c.name == chain; });
    const bool is_present = found != rig->chains.end();
    if (!is_present) {
      return std::unexpected(NoChain(doll, chain));
    }
    return change(&*found);
  });
}

// Drops warp keys of piece on every layer showing doll.
inline Project WithoutWarpKeys(Project project, const QString& doll,
                        const QString& piece) {
  assert(!doll.isEmpty() && !piece.isEmpty());
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  for (auto& shared : project.shots) {
    Shot shot = *shared;
    bool is_changed = false;
    for (Layer& layer : shot.layers) {
      auto* posed = std::get_if<DollLayer>(&layer.content);
      const bool is_match = posed != nullptr && posed->doll == doll &&
                            posed->pieces.contains(piece);
      if (!is_match) {
        continue;
      }
      for (Key<PiecePose>& key : posed->pieces.at(piece).keys) {
        is_changed = is_changed || !key.value.warp.empty();
        key.value.warp.clear();
      }
    }
    if (is_changed) {
      shared = std::make_shared<const Shot>(std::move(shot));
    }
  }
  return project;
}

// "1 dot" or "6 dots", for the names of many-dot steps.
inline QString CountText(size_t count) {
  assert(count > 0);
  assert(count < 10000000);
  const bool is_one = count == 1;
  return is_one ? Tr("1 dot") : Tr("%1 dots").arg(count);
}

// change(rig, point) on each of dots; the first refusal stops it. A dot
// with a layer is changed in that layer's own copy of the piece's dots
// (change sees them in the fields of a piece and what it leaves there is
// kept for the layer); one with none changes the doll's default.
template <typename Change>
Result<Project> WithDots(const Project& project,
                         const std::vector<DotRef>& dots, Change change) {
  assert(dots.size() < 10000000);
  assert(!dots.empty());
  Result<Project> next = project;
  for (const DotRef& dot : dots) {
    next = WithPiece(*next, dot.doll, dot.piece,
                     [&](RigPiece* rig) -> Result<void> {
                       const bool is_point =
                           dot.point >= 0 &&
                           dot.point < rig->warp.PointCount();
                       if (!is_point) {
                         return std::unexpected(Error{Tr(
                             "Give the piece a warp grid in the rig "
                             "first.")});
                       }
                       const bool is_layer = dot.layer.IsValid();
                       if (!is_layer) {
                         return change(rig, dot.point);
                       }
                       RigPiece shown = PieceFor(*rig, dot.layer);
                       const Result<void> done = change(&shown, dot.point);
                       if (done) {
                         rig->layer_dots[dot.layer] = {
                             shown.warp_reach, shown.drag_nodes,
                             shown.point_motions};
                       }
                       return done;
                     });
    const bool is_refused = !next;
    if (is_refused) {
      return next;
    }
  }
  return next;
}

// The refusal of a many-dot edit given no dots.
inline Result<void> NeedDots() {
  assert(kMaxWarpPoints > 0);
  assert(kMaxIkChains > 0);
  return std::unexpected(Error{Tr("Pick one or more warp dots first.")});
}

// The drag node on point of rig, or nullptr.
inline DragNode* FindNode(RigPiece* rig, int point) {
  assert(rig != nullptr);
  assert(point >= 0);
  for (DragNode& node : rig->drag_nodes) {
    const bool is_point = node.point == point;
    if (is_point) {
      return &node;
    }
  }
  return nullptr;
}

}  // namespace snapper

#endif  // SNAPPER_EDIT_RIG_EDITS_H_
