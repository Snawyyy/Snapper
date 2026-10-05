#include "anim/ik.h"

#include <QLineF>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

namespace snapper {
namespace {

constexpr double kDegrees = 180.0 / std::numbers::pi;
// Keeps the solve away from fully straight or folded bones, where the
// bend direction would flip from frame to frame.
constexpr double kReachSlack = 1e-6;

double AngleOf(QPointF from, QPointF to) {
  assert(std::isfinite(from.x()) && std::isfinite(to.x()));
  assert(std::isfinite(from.y()) && std::isfinite(to.y()));
  return std::atan2(to.y() - from.y(), to.x() - from.x());
}

// Shortest signed turn from a to b, in radians.
double Turn(double a, double b) {
  assert(std::isfinite(a) && std::isfinite(b));
  const double turn = std::remainder(b - a, 2.0 * std::numbers::pi);
  assert(std::abs(turn) <= std::numbers::pi + 1e-9);
  return turn;
}

// A mirrored parent turns its children the other way on screen. A
// piece's own scale comes before its turn, so only the parent counts.
double Handedness(const std::map<QString, QTransform>& transforms,
                  const QString& parent) {
  assert(transforms.size() <= static_cast<size_t>(kMaxDollPieces));
  const auto found = transforms.find(parent);
  const bool is_root = parent.isEmpty() || found == transforms.end();
  if (is_root) {
    return 1.0;
  }
  assert(std::isfinite(found->second.determinant()));
  return found->second.determinant() < 0.0 ? -1.0 : 1.0;
}

double RotationOf(const PoseMap& poses, const QString& name) {
  assert(!name.isEmpty());
  assert(poses.size() <= static_cast<size_t>(kMaxPieceTracks));
  const auto found = poses.find(name);
  return found == poses.end() ? 0.0 : found->second.rotation;
}

// The chain as it stands, in doll space.
struct Bones final {
  QPointF shoulder;
  QPointF elbow;
  QPointF tip;
  double upper_length = 0.0;
  double lower_length = 0.0;
  double upper_hand = 1.0;
  double lower_hand = 1.0;
};

std::optional<Bones> Measure(const Doll& doll, const PoseMap& poses,
                             const IkChain& chain) {
  assert(!chain.upper.isEmpty() && !chain.lower.isEmpty());
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const auto transforms = PieceTransforms(doll, poses);
  const RigPiece* upper = FindRig(doll.rig, chain.upper);
  const RigPiece* lower = FindRig(doll.rig, chain.lower);
  const bool is_complete = upper != nullptr && lower != nullptr &&
                           transforms.contains(chain.upper) &&
                           transforms.contains(chain.lower);
  if (!is_complete) {
    return std::nullopt;
  }
  Bones bones;
  bones.shoulder = transforms.at(chain.upper).map(upper->pivot);
  bones.elbow = transforms.at(chain.lower).map(lower->pivot);
  bones.tip = transforms.at(chain.lower).map(chain.tip);
  bones.upper_length = QLineF(bones.shoulder, bones.elbow).length();
  bones.lower_length = QLineF(bones.elbow, bones.tip).length();
  bones.upper_hand = Handedness(transforms, upper->parent);
  bones.lower_hand = Handedness(transforms, lower->parent);
  const bool has_bones = bones.upper_length > 0.0 && bones.lower_length > 0.0;
  if (!has_bones) {
    return std::nullopt;
  }
  return bones;
}

}  // namespace

std::optional<IkSolution> SolveIk(const Doll& doll, const PoseMap& poses,
                                  const IkChain& chain, QPointF target) {
  assert(std::isfinite(target.x()) && std::isfinite(target.y()));
  assert(doll.rig.chains.size() <= static_cast<size_t>(kMaxIkChains));
  const std::optional<Bones> bones = Measure(doll, poses, chain);
  if (!bones) {
    return std::nullopt;
  }
  const double wanted = QLineF(bones->shoulder, target).length();
  const double shortest = std::abs(bones->upper_length - bones->lower_length);
  const double longest = bones->upper_length + bones->lower_length;
  const double reach = std::clamp(wanted, shortest + kReachSlack,
                                  longest - kReachSlack);
  // Law of cosines: the angle at the shoulder between the target and
  // the upper bone.
  const double up = bones->upper_length;
  const double low = bones->lower_length;
  const double cosine = (up * up + reach * reach - low * low) /
                        (2.0 * up * reach);
  const double side = chain.bends_clockwise ? -1.0 : 1.0;
  const double aim = AngleOf(bones->shoulder, target);
  const double upper_angle =
      aim + side * std::acos(std::clamp(cosine, -1.0, 1.0));
  const double upper_turn =
      Turn(AngleOf(bones->shoulder, bones->elbow), upper_angle);
  // Where the elbow and the reachable target end up after that turn.
  const QPointF new_elbow =
      bones->shoulder +
      QPointF(std::cos(upper_angle), std::sin(upper_angle)) * up;
  const QPointF goal =
      bones->shoulder + QPointF(std::cos(aim), std::sin(aim)) * reach;
  const double lower_turn =
      Turn(AngleOf(bones->elbow, bones->tip) + upper_turn,
           AngleOf(new_elbow, goal));

  IkSolution solution;
  solution.upper_rotation = RotationOf(poses, chain.upper) +
                            upper_turn * kDegrees * bones->upper_hand;
  solution.lower_rotation = RotationOf(poses, chain.lower) +
                            lower_turn * kDegrees * bones->lower_hand;
  solution.is_reached = std::abs(wanted - reach) < 1e-3;
  return solution;
}

}  // namespace snapper
