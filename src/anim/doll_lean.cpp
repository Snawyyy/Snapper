#include "anim/doll_lean.h"

#include <QRectF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>

namespace snapper {
namespace {

// One box around every placed piece's drawing, in doll space.
QRectF BoxOf(const Doll& doll,
             const std::map<QString, QTransform>& placed) {
  assert(placed.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(doll.art.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  QRectF box;
  for (const auto& [name, transform] : placed) {
    const ArtPiece* art = FindArt(doll, name);
    const bool has_art = art != nullptr;
    if (has_art) {
      box = box.united(
          transform.mapRect(QRectF(QPointF(), QSizeF(art->size))));
    }
  }
  return box;
}

// How much each piece grows: 1 + amount * its weight, the weight read
// off its rest joint's height in the rest box.
std::map<QString, double> Factors(const Doll& doll, double amount) {
  assert(std::abs(amount) <= kMaxLean);
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const std::map<QString, QTransform> rest = PieceTransforms(doll, {});
  const QRectF box = BoxOf(doll, rest);
  const double half = box.height() / 2.0;
  std::map<QString, double> factors;
  for (const RigPiece& rig : doll.rig.pieces) {
    const auto placed = rest.find(rig.name);
    const bool is_placed = placed != rest.end() && half > 0.0;
    if (is_placed) {
      const double joint_y = placed->second.map(rig.pivot).y();
      const double weight =
          std::clamp((box.center().y() - joint_y) / half, -1.0, 1.0);
      factors[rig.name] = 1.0 + amount * weight;
    }
  }
  return factors;
}

}  // namespace

PoseMap LeanPoses(const Doll& doll, const PoseMap& poses, double amount) {
  assert(std::isfinite(amount));
  assert(poses.size() <= static_cast<size_t>(kMaxPieceTracks));
  const double lean = std::clamp(amount, -kMaxLean, kMaxLean);
  const std::map<QString, double> factors = Factors(doll, lean);
  const std::map<QString, QTransform> placed = PieceTransforms(doll, poses);
  const QPointF middle = BoxOf(doll, placed).center();
  PoseMap leaned = poses;
  for (const RigPiece& rig : doll.rig.pieces) {
    const ArtPiece* art = FindArt(doll, rig.name);
    const auto factor = factors.find(rig.name);
    const bool is_leaned = art != nullptr && factor != factors.end();
    if (!is_leaned) {
      continue;
    }
    // Each piece should end up scaled by its factor around the middle,
    // in doll space. Its parent already brings the parent's factor, so
    // the piece adds the ratio, around the middle as seen from inside
    // the parent's motion.
    const bool is_root = rig.parent.isEmpty();
    const auto parent = placed.find(rig.parent);
    const ArtPiece* parent_art =
        is_root ? nullptr : FindArt(doll, rig.parent);
    const bool has_parent = parent != placed.end() &&
                            parent_art != nullptr &&
                            factors.contains(rig.parent);
    double ratio = factor->second;
    QPointF around = middle;
    if (has_parent) {
      ratio /= factors.at(rig.parent);
      around = parent->second.inverted().map(middle) + parent_art->position;
    }
    // Scaling commutes with the piece's own turn, skew and scale, so it
    // folds into them and the offset.
    PiecePose& pose = leaned[rig.name];
    const QPointF joint = art->position + rig.pivot;
    pose.scale_x *= ratio;
    pose.scale_y *= ratio;
    pose.offset = (joint + pose.offset - around) * ratio + around - joint;
  }
  return leaned;
}

}  // namespace snapper
