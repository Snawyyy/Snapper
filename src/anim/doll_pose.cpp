#include "anim/doll_pose.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

#include "anim/sampler.h"
#include "anim/warp.h"

namespace snapper {
namespace {

PiecePose PoseOf(const PoseMap& poses, const QString& name) {
  assert(!name.isEmpty());
  assert(poses.size() <= static_cast<size_t>(kMaxPieceTracks));
  const auto found = poses.find(name);
  return found == poses.end() ? PiecePose() : found->second;
}

// How the piece's own pose moves doll space at rest: around its joint,
// from its rest angle. Its parent's motion is added on top.
QTransform OwnMotion(const ArtPiece& art, const RigPiece& rig,
                     const PoseMap& poses) {
  assert(!art.name.isEmpty());
  assert(art.name == rig.name);
  PiecePose pose = PoseOf(poses, art.name);
  pose.rotation += rig.rest_rotation;
  return PoseMatrix(pose, art.position + rig.pivot);
}

}  // namespace

QTransform PoseMatrix(const PiecePose& pose, QPointF center) {
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

PoseMap SamplePoses(const DollLayer& layer, Frame frame) {
  assert(frame.index() >= 0);
  assert(layer.pieces.size() <= static_cast<size_t>(kMaxPieceTracks));
  PoseMap poses;
  for (const auto& [name, channel] : layer.pieces) {
    poses[name] = Sample(channel, frame, PiecePose());
  }
  return poses;
}

std::map<QString, QTransform> PieceTransforms(const Doll& doll,
                                              const PoseMap& poses) {
  assert(doll.art.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  // Rest doll space to posed doll space, per piece: its own motion,
  // then its parent's. Each pass places every piece whose parent is
  // placed; a tree of n pieces needs at most n passes.
  std::map<QString, QTransform> motion;
  for (int pass = 0; pass <= kMaxDollPieces; ++pass) {
    bool placed_any = false;
    for (const RigPiece& rig : doll.rig.pieces) {
      const ArtPiece* art = FindArt(doll, rig.name);
      const bool is_skipped = art == nullptr || motion.contains(rig.name);
      if (is_skipped) {
        continue;
      }
      const bool has_parent = !rig.parent.isEmpty() &&
                              FindRig(doll.rig, rig.parent) != nullptr &&
                              FindArt(doll, rig.parent) != nullptr;
      const bool is_waiting = has_parent && !motion.contains(rig.parent);
      if (is_waiting) {
        continue;
      }
      QTransform moved = OwnMotion(*art, rig, poses);
      if (has_parent) {
        moved = moved * motion.at(rig.parent);
      }
      motion[rig.name] = moved;
      placed_any = true;
    }
    if (!placed_any) {
      break;
    }
  }
  // A drawing's pixels sit at its Krita spot, then move with it.
  std::map<QString, QTransform> placed;
  for (const auto& [name, moved] : motion) {
    const QPointF at = FindArt(doll, name)->position;
    placed[name] = QTransform::fromTranslate(at.x(), at.y()) * moved;
  }
  return placed;
}

std::vector<PlacedPiece> PlaceDoll(const Doll& doll, const PoseMap& poses) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(poses.size() <= static_cast<size_t>(kMaxPieceTracks));
  const std::map<QString, QTransform> transforms =
      PieceTransforms(doll, poses);
  std::vector<PlacedPiece> placed;
  for (const RigPiece& rig : doll.rig.pieces) {
    const ArtPiece* art = FindArt(doll, rig.name);
    const auto transform = transforms.find(rig.name);
    const bool is_placed = art != nullptr && transform != transforms.end();
    if (!is_placed) {
      continue;
    }
    const PiecePose pose = PoseOf(poses, rig.name);
    PlacedPiece piece{rig.name,
                      DrawingPath(doll, rig.name, pose.drawing),
                      art->size,
                      transform->second,
                      rig.order,
                      pose.opacity,
                      rig.warp,
                      {}};
    const bool is_warped =
        rig.warp.IsOn() &&
        static_cast<int>(pose.warp.size()) == rig.warp.PointCount();
    if (is_warped) {
      piece.warp_points = WarpedPoints(art->size, rig.warp, pose.warp);
    }
    placed.push_back(std::move(piece));
  }
  std::stable_sort(placed.begin(), placed.end(),
                   [](const PlacedPiece& a, const PlacedPiece& b) {
                     return a.order < b.order;
                   });
  return placed;
}

}  // namespace snapper
