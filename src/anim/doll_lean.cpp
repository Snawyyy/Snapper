#include "anim/doll_lean.h"

#include <QRectF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <vector>
#include <numbers>

namespace snapper {
namespace {

constexpr double kRadians = std::numbers::pi / 180.0;

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

// Each piece's rest joint height above the middle line of the rest
// box, and the doll's rest height.
struct RestHeights final {
  std::map<QString, double> above;
  // The same for the middle of each piece's drawing.
  std::map<QString, double> middle_above;
  double doll = 0.0;
};

RestHeights HeightsOf(const Doll& doll) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const std::map<QString, QTransform> rest = PieceTransforms(doll, {});
  const QRectF box = BoxOf(doll, rest);
  RestHeights heights;
  heights.doll = box.height();
  for (const RigPiece& rig : doll.rig.pieces) {
    const auto placed = rest.find(rig.name);
    const bool is_placed = placed != rest.end();
    if (is_placed) {
      heights.above[rig.name] =
          box.center().y() - placed->second.map(rig.pivot).y();
      const ArtPiece* art = FindArt(doll, rig.name);
      heights.middle_above[rig.name] =
          box.center().y() -
          placed->second.map(QRectF(QPointF(), QSizeF(art->size)).center())
              .y();
    }
  }
  assert(heights.above.size() <= rest.size());
  return heights;
}

// How much nearer the camera brings each piece once the doll tips by
// tilt: its rest height off the middle line sets how far it swings
// toward (above the line) or away from (below) the camera.
std::map<QString, double> Growth(const RestHeights& heights, double tilt) {
  assert(std::abs(tilt) <= kMaxTilt * kRadians);
  assert(heights.doll >= 0.0);
  const double distance = kCameraDistance * heights.doll;
  std::map<QString, double> growth;
  const bool has_height = distance > 0.0;
  if (!has_height) {
    return growth;
  }
  for (const auto& [name, above] : heights.above) {
    const double nearer = above * std::sin(tilt);
    growth[name] = distance / std::max(distance - nearer, distance / 10);
  }
  return growth;
}

// Brings pieces nearer the camera in front. A piece only passes its
// parent, its children and its siblings, and only when clearly nearer
// (by kRestackGap of the doll's height, measured at the middles of
// their drawings); otherwise the order is kept, so hair stays behind
// the body and close pieces such as hair and face stay as rigged.
void Restack(const Doll& doll, const RestHeights& heights, double tilt,
             PoseMap* leaned) {
  assert(leaned != nullptr);
  assert(std::isfinite(tilt));
  const bool is_level = tilt == 0.0;
  if (is_level) {
    return;
  }
  struct Stacked final {
    double nearer;
    int order;
    const RigPiece* rig;
  };
  std::vector<Stacked> left;
  for (const RigPiece& rig : doll.rig.pieces) {
    const auto above = heights.middle_above.find(rig.name);
    const bool is_placed = above != heights.middle_above.end();
    if (is_placed) {
      left.push_back({above->second * (tilt > 0.0 ? 1.0 : -1.0),
                      rig.order + (*leaned)[rig.name].order, &rig});
    }
  }
  const double gap = kRestackGap * heights.doll;
  // Back to front: each time, of the pieces nothing left must go
  // behind, the one the rig and pose draw furthest back.
  for (int rank = 0; !left.empty() && rank < kMaxDollPieces; ++rank) {
    auto next = left.end();
    for (auto it = left.begin(); it != left.end(); ++it) {
      const bool is_free =
          std::none_of(left.begin(), left.end(), [&](const Stacked& other) {
            const RigPiece& a = *it->rig;
            const RigPiece& b = *other.rig;
            const bool is_kin = a.parent == b.name || b.parent == a.name ||
                                a.parent == b.parent;
            return is_kin && it->nearer - other.nearer > gap;
          });
      const bool is_better = is_free && (next == left.end() ||
                                         it->order < next->order);
      if (is_better) {
        next = it;
      }
    }
    (*leaned)[next->rig->name].order = rank - next->rig->order;
    left.erase(next);
  }
}

// Writes own (rest doll space to the piece's place, before its
// parent's motion) into pose as turn, scale, skew and offset around
// joint, the way PoseMatrix builds them. Turns stay near pose's old
// turn and a flipped piece stays flipped.
void Unbuild(const QTransform& own, QPointF joint, double rest_rotation,
             PiecePose* pose) {
  assert(pose != nullptr);
  assert(std::isfinite(joint.x()) && std::isfinite(joint.y()));
  const double sign = pose->scale_x < 0.0 ? -1.0 : 1.0;
  const double scale_x = sign * std::hypot(own.m11(), own.m12());
  const bool is_flat = std::abs(scale_x) < 1e-9;
  if (is_flat) {
    return;
  }
  const double cos = own.m11() / scale_x;
  const double sin = own.m12() / scale_x;
  const double scale_y = -own.m21() * sin + own.m22() * cos;
  const double turn = std::atan2(sin, cos) / kRadians - rest_rotation;
  pose->rotation += std::remainder(turn - pose->rotation, 360.0);
  pose->scale_x = scale_x;
  pose->scale_y = scale_y;
  const bool has_height = std::abs(scale_y) > 1e-9;
  if (has_height) {
    const double shear = (own.m21() * cos + own.m22() * sin) / scale_y;
    pose->skew = std::atan(shear) / kRadians;
  }
  pose->offset = own.map(joint) - joint;
}

}  // namespace

PoseMap LeanPoses(const Doll& doll, const PoseMap& poses, double amount) {
  assert(std::isfinite(amount));
  assert(poses.size() <= static_cast<size_t>(kMaxPieceTracks));
  const double tilt =
      std::clamp(amount, -kMaxLean, kMaxLean) / kMaxLean * kMaxTilt *
      kRadians;
  const RestHeights heights = HeightsOf(doll);
  const std::map<QString, double> growth = Growth(heights, tilt);
  const std::map<QString, QTransform> placed = PieceTransforms(doll, poses);
  const QPointF middle = BoxOf(doll, placed).center();
  // Seen tipped, heights shrink by the cosine around the middle.
  const double squash = std::cos(tilt);
  PoseMap leaned = poses;
  std::map<QString, QTransform> moved;
  for (int pass = 0; pass <= kMaxDollPieces; ++pass) {
    bool moved_any = false;
    for (const RigPiece& rig : doll.rig.pieces) {
      const auto at = placed.find(rig.name);
      const auto grow = growth.find(rig.name);
      const bool is_root = rig.parent.isEmpty();
      const bool has_parent = !is_root && placed.contains(rig.parent);
      const bool is_ready = at != placed.end() && grow != growth.end() &&
                            !moved.contains(rig.name) &&
                            (!has_parent || moved.contains(rig.parent));
      if (!is_ready) {
        continue;
      }
      // The joint lands where the camera would see it: nearer is
      // bigger and further out, heights squash toward the middle. The
      // drawing only grows, so faces and hands keep their shape.
      const QPointF joint = at->second.map(rig.pivot);
      const QPointF seen =
          middle + QPointF((joint.x() - middle.x()) * grow->second,
                           (joint.y() - middle.y()) * grow->second * squash);
      const QTransform target =
          at->second * QTransform::fromTranslate(-joint.x(), -joint.y()) *
          QTransform::fromScale(grow->second, grow->second) *
          QTransform::fromTranslate(seen.x(), seen.y());
      const QPointF corner = FindArt(doll, rig.name)->position;
      const QTransform motion =
          QTransform::fromTranslate(-corner.x(), -corner.y()) * target;
      const QTransform own =
          has_parent ? motion * moved.at(rig.parent).inverted() : motion;
      moved[rig.name] = motion;
      Unbuild(own, corner + rig.pivot, rig.rest_rotation,
              &leaned[rig.name]);
      moved_any = true;
    }
    if (!moved_any) {
      break;
    }
  }
  Restack(doll, heights, tilt, &leaned);
  return leaned;
}

}  // namespace snapper
