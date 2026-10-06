#include "anim/doll_lean.h"

#include <QRectF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <variant>
#include <vector>

#include "anim/doll_drag.h"
#include "anim/sampler.h"
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

// Each piece's rest joint and drawing middle, from the middle of the
// rest box (x to the right, height up), and the doll's rest height.
struct RestSpots final {
  std::map<QString, QPointF> joint;
  std::map<QString, QPointF> middle;
  double doll = 0.0;
};

RestSpots SpotsOf(const Doll& doll) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const std::map<QString, QTransform> rest = PieceTransforms(doll, {});
  const QRectF box = BoxOf(doll, rest);
  const auto from_middle = [&box](QPointF point) {
    return QPointF(point.x() - box.center().x(), box.center().y() - point.y());
  };
  RestSpots spots;
  spots.doll = box.height();
  for (const RigPiece& rig : doll.rig.pieces) {
    const auto placed = rest.find(rig.name);
    const bool is_placed = placed != rest.end();
    if (is_placed) {
      const ArtPiece* art = FindArt(doll, rig.name);
      spots.joint[rig.name] = from_middle(placed->second.map(rig.pivot));
      spots.middle[rig.name] = from_middle(placed->second.map(
          QRectF(QPointF(), QSizeF(art->size)).center()));
    }
  }
  assert(spots.joint.size() <= rest.size());
  return spots;
}

// The doll turned: lean tips the top toward the camera, swivel the
// right side.
struct Turn final {
  double lean_cos;
  double lean_sin;
  double swivel_cos;
  double swivel_sin;

  // How much nearer the camera a spot (x right, height up, from the
  // middle) comes.
  double Nearer(QPointF spot) const {
    return spot.x() * swivel_sin * lean_cos + spot.y() * lean_sin;
  }
};

// How much nearer the camera brings each piece once turned, as a size:
// near pieces grow, far ones shrink.
std::map<QString, double> Growth(const RestSpots& spots, const Turn& turn) {
  assert(spots.doll >= 0.0);
  assert(std::isfinite(turn.lean_cos) && std::isfinite(turn.swivel_cos));
  const double distance = kCameraDistance * spots.doll;
  std::map<QString, double> growth;
  const bool has_height = distance > 0.0;
  if (!has_height) {
    return growth;
  }
  for (const auto& [name, joint] : spots.joint) {
    growth[name] =
        distance / std::max(distance - turn.Nearer(joint), distance / 10);
  }
  return growth;
}

// Brings pieces nearer the camera in front. A piece only passes its
// parent, its children and its siblings, and only when clearly nearer
// (by kRestackGap of the doll's height, measured at the middles of
// their drawings); otherwise the order is kept, so hair stays behind
// the body and close pieces such as hair and face stay as rigged.
void Restack(const Doll& doll, const RestSpots& spots, const Turn& turn,
             PoseMap* leaned) {
  assert(leaned != nullptr);
  assert(spots.doll >= 0.0);
  // Nearness is measured along the way the doll turns, so any turn
  // however small restacks; seen from behind, the rig's order turns
  // round.
  const double reach =
      std::hypot(turn.swivel_sin * turn.lean_cos, turn.lean_sin);
  const double toward = reach > 1e-9 ? 1.0 / reach : 0.0;
  const int facing = turn.lean_cos * turn.swivel_cos < 0.0 ? -1 : 1;
  struct Stacked final {
    double nearer;
    int order;
    const RigPiece* rig;
  };
  std::vector<Stacked> left;
  for (const RigPiece& rig : doll.rig.pieces) {
    const auto middle = spots.middle.find(rig.name);
    const bool is_placed = middle != spots.middle.end();
    if (is_placed) {
      left.push_back({turn.Nearer(middle->second) * toward,
                      facing * (rig.order + (*leaned)[rig.name].order),
                      &rig});
    }
  }
  const double gap = kRestackGap * spots.doll;
  // Back to front: each time, of the pieces nothing left must go
  // behind, the one the rig and pose draw furthest back. A piece must
  // go behind kin that is clearly nearer, and behind close kin drawn
  // further back. Should those ever knot up, the furthest back goes.
  const auto must_wait = [gap](const Stacked& piece, const Stacked& other) {
    const RigPiece& a = *piece.rig;
    const RigPiece& b = *other.rig;
    const bool is_kin = a.parent == b.name || b.parent == a.name ||
                        a.parent == b.parent;
    const double nearer = piece.nearer - other.nearer;
    const bool is_close = std::abs(nearer) <= gap;
    return is_kin && (nearer > gap || (is_close && other.order < piece.order));
  };
  for (int rank = 0; !left.empty() && rank < kMaxDollPieces; ++rank) {
    auto next = left.end();
    auto backmost = left.begin();
    for (auto it = left.begin(); it != left.end(); ++it) {
      const bool is_free = std::none_of(
          left.begin(), left.end(),
          [&](const Stacked& other) { return must_wait(*it, other); });
      const bool is_better = is_free && (next == left.end() ||
                                         it->order < next->order);
      if (is_better) {
        next = it;
      }
      const bool is_further = it->order < backmost->order;
      if (is_further) {
        backmost = it;
      }
    }
    const bool is_knotted = next == left.end();
    if (is_knotted) {
      next = backmost;
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

// Keeps a cosine off zero, sign and all, so a doll turned edge on
// keeps a sliver of size and can still be undone.
double Sliver(double cos) {
  assert(std::isfinite(cos));
  const double sliver = std::copysign(std::max(std::abs(cos), 0.01), cos);
  assert(sliver != 0.0);
  return sliver;
}

// How the turn moves spots (pixels from the middle, y down) on screen,
// before nearness grows them: widths shrink by the swivel, heights by
// the lean, and both together slant.
QTransform Spacing(const Turn& turn) {
  assert(std::isfinite(turn.lean_sin));
  assert(std::isfinite(turn.swivel_sin));
  return QTransform(Sliver(turn.swivel_cos),
                    turn.swivel_sin * turn.lean_sin, 0.0,
                    Sliver(turn.lean_cos), 0.0, 0.0);
}

// How a drawing changes shape: as Spacing, but squashing less
// (kDrawingSquash) so it doesn't look like paper, and only turning
// over, never squashing, when the piece keeps its shape.
QTransform DrawingShape(const Turn& turn, bool keeps_shape) {
  assert(std::isfinite(turn.lean_cos));
  assert(std::isfinite(turn.swivel_cos));
  const auto squash = [keeps_shape](double cos) {
    return keeps_shape
               ? std::copysign(1.0, cos)
               : std::copysign(std::pow(std::abs(Sliver(cos)),
                                        kDrawingSquash),
                               cos);
  };
  const double slant =
      keeps_shape ? 0.0 : turn.swivel_sin * turn.lean_sin;
  return QTransform(squash(turn.swivel_cos), slant, 0.0,
                    squash(turn.lean_cos), 0.0, 0.0);
}

}  // namespace

PoseMap LeanPoses(const Doll& doll, const PoseMap& poses, double lean,
                  double swivel) {
  assert(std::isfinite(lean) && std::isfinite(swivel));
  assert(poses.size() <= static_cast<size_t>(kMaxPieceTracks));
  // A whole turn comes back to the start, exactly.
  const double tip = std::remainder(lean, 360.0) * kRadians;
  const double side = std::remainder(swivel, 360.0) * kRadians;
  const bool is_level = tip == 0.0 && side == 0.0;
  if (is_level) {
    return poses;
  }
  const Turn turn{std::cos(tip), std::sin(tip), std::cos(side),
                  std::sin(side)};
  const RestSpots spots = SpotsOf(doll);
  const std::map<QString, double> growth = Growth(spots, turn);
  const std::map<QString, QTransform> placed = PieceTransforms(doll, poses);
  const QPointF middle = BoxOf(doll, placed).center();
  const QTransform spacing = Spacing(turn);
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
      // The joint lands where the camera would see it, nearer pieces
      // bigger and further out, and the drawing changes shape with it.
      const QPointF joint = at->second.map(rig.pivot);
      const QPointF seen =
          middle + spacing.map(joint - middle) * grow->second;
      const QTransform target =
          at->second * QTransform::fromTranslate(-joint.x(), -joint.y()) *
          DrawingShape(turn, rig.keeps_shape) *
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
  Restack(doll, spots, turn, &leaned);
  return leaned;
}

PoseMap ShownPoses(const Doll& doll, const Layer& layer, Frame frame) {
  assert(frame.index() >= 0);
  assert(std::holds_alternative<DollLayer>(layer.content));
  const PoseMap keyed = DraggedPoses(doll, layer, frame);
  const PiecePose own = Sample(layer.transform, frame, PiecePose());
  const bool is_turned = std::isfinite(own.lean) &&
                         std::isfinite(own.swivel) &&
                         (own.lean != 0.0 || own.swivel != 0.0);
  return is_turned ? LeanPoses(doll, keyed, own.lean, own.swivel) : keyed;
}

}  // namespace snapper
