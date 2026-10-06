#include "anim/doll_lean.h"

#include <QRectF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
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

// The doll's main parts: going down from the roots while there is only
// one branch, each piece belongs to the first branch that splits off.
// Pieces above the split are parts of their own.
std::map<QString, QString> PartsOf(const Doll& doll) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const auto is_hung = [&doll](const RigPiece& rig) {
    return !rig.parent.isEmpty() && FindRig(doll.rig, rig.parent) != nullptr;
  };
  std::map<QString, QString> parts;
  std::set<QString> level;
  for (const RigPiece& rig : doll.rig.pieces) {
    const bool is_root = !is_hung(rig);
    if (is_root) {
      level.insert(rig.name);
    }
  }
  for (int depth = 0; depth < kMaxDollPieces && level.size() == 1; ++depth) {
    const QString single = *level.begin();
    parts[single] = single;
    level.clear();
    for (const RigPiece& rig : doll.rig.pieces) {
      const bool is_child = is_hung(rig) && rig.parent == single;
      if (is_child) {
        level.insert(rig.name);
      }
    }
  }
  for (const RigPiece& rig : doll.rig.pieces) {
    const RigPiece* at = &rig;
    for (int i = 0; i < kMaxDollPieces && !parts.contains(rig.name); ++i) {
      const bool is_head = level.contains(at->name);
      if (is_head) {
        parts[rig.name] = at->name;
      }
      const bool is_top = !is_hung(*at);
      if (is_top) {
        break;
      }
      at = FindRig(doll.rig, at->parent);
    }
  }
  assert(parts.size() <= doll.rig.pieces.size());
  return parts;
}

// Brings the main parts nearer the camera in front, keeping the order
// inside each part as it was.
void Restack(const Doll& doll, const RestHeights& heights, double tilt,
             PoseMap* leaned) {
  assert(leaned != nullptr);
  assert(std::isfinite(tilt));
  const bool is_level = tilt == 0.0;
  if (is_level) {
    return;
  }
  const std::map<QString, QString> parts = PartsOf(doll);
  struct Stacked final {
    double nearer;
    int order;
    int index;
    const RigPiece* rig;
  };
  std::vector<Stacked> stack;
  for (int i = 0; i < static_cast<int>(doll.rig.pieces.size()); ++i) {
    const RigPiece& rig = doll.rig.pieces[static_cast<size_t>(i)];
    const auto part = parts.find(rig.name);
    const auto above = part != parts.end() ? heights.above.find(part->second)
                                           : heights.above.end();
    const bool is_placed = above != heights.above.end();
    if (is_placed) {
      stack.push_back({above->second * (tilt > 0.0 ? 1.0 : -1.0),
                       rig.order + (*leaned)[rig.name].order, i, &rig});
    }
  }
  std::sort(stack.begin(), stack.end(),
            [](const Stacked& a, const Stacked& b) {
              return std::tie(a.nearer, a.order, a.index) <
                     std::tie(b.nearer, b.order, b.index);
            });
  for (int rank = 0; rank < static_cast<int>(stack.size()); ++rank) {
    const RigPiece& rig = *stack[static_cast<size_t>(rank)].rig;
    (*leaned)[rig.name].order = rank - rig.order;
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
