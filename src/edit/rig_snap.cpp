// RigManager's joint snap and joint circles: joints moved onto the
// middle of their drawn limb, and the disc that fills a bent joint.

#include <QImage>
#include <QTransform>

#include <cassert>
#include <cmath>
#include <map>
#include <optional>

#include "anim/doll_pose.h"
#include "anim/joint_snap.h"
#include "edit/history_manager.h"
#include "edit/rig_edits.h"
#include "edit/rig_manager.h"

namespace snapper {
namespace {

// A joint closer than this (drawing pixels) to its snap stays put.
constexpr double kSnapStill = 0.5;

// Where piece's joint should go, as an offset from where it is; none
// when it is on no shape or already there.
std::optional<QPointF> SnapOffset(
    const RigPiece& piece, const std::map<QString, QImage>& drawings,
    const std::map<QString, QTransform>& transforms) {
  assert(!piece.name.isEmpty());
  assert(drawings.size() <= static_cast<size_t>(kMaxDollPieces));
  const auto drawing = drawings.find(piece.name);
  if (drawing == drawings.end()) {
    return std::nullopt;
  }
  const LimbMask own = MeasureLimbs(drawing->second);
  const auto parent_drawing = drawings.find(piece.parent);
  const auto own_at = transforms.find(piece.name);
  const auto parent_at = transforms.find(piece.parent);
  const bool has_parent = !piece.parent.isEmpty() &&
                          parent_drawing != drawings.end() &&
                          own_at != transforms.end() &&
                          parent_at != transforms.end();
  const LimbMask parent =
      has_parent ? MeasureLimbs(parent_drawing->second) : LimbMask{};
  bool is_invertible = false;
  const QTransform parent_to_own =
      has_parent ? parent_at->second * own_at->second.inverted(&is_invertible)
                 : QTransform();
  const bool uses_parent = has_parent && is_invertible;
  const auto snap = NearestShapeSnap(own, uses_parent ? &parent : nullptr,
                                     parent_to_own, piece.pivot);
  const QPointF offset = snap ? snap->spot - piece.pivot : QPointF();
  const bool moves = std::hypot(offset.x(), offset.y()) >= kSnapStill;
  return moves ? std::optional<QPointF>(offset) : std::nullopt;
}

}  // namespace

Result<std::vector<QString>> RigManager::SnapJointsToShape(
    const QString& doll, const std::vector<QString>& pieces,
    const std::map<QString, QImage>& drawings) {
  assert(history_ != nullptr);
  assert(pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const Doll* found = FindDoll(history_->current(), doll);
  if (found == nullptr) {
    return std::unexpected(Error{Tr("%1 isn't in the project.").arg(doll)});
  }
  const auto transforms = PieceTransforms(*found, PoseMap());
  std::map<QString, QPointF> offsets;
  std::vector<QString> moved;
  for (const QString& name : pieces) {
    const RigPiece* piece = FindRig(found->rig, name);
    const auto offset = piece != nullptr
                            ? SnapOffset(*piece, drawings, transforms)
                            : std::nullopt;
    if (offset) {
      offsets[name] = *offset;
      moved.push_back(name);
    }
  }
  if (moved.empty()) {
    return std::unexpected(Error{
        Tr("No joint moved: each is already on its shape or off it.")});
  }
  const QString label = moved.size() == 1
                            ? Tr("Snap joint of %1").arg(moved.front())
                            : Tr("Snap %1 joints").arg(moved.size());
  const Result<void> done = history_->Apply(
      label, WithRig(history_->current(), doll, [&offsets](Rig* rig) {
        for (const auto& [name, offset] : offsets) {
          RigPiece* piece = FindRig(rig, name);
          assert(piece != nullptr);
          piece->pivot += offset;
        }
        return Result<void>();
      }));
  if (!done) {
    return std::unexpected(done.error());
  }
  return moved;
}

Result<void> RigManager::ToggleJointCircle(const QString& doll,
                                           const QString& piece) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const RigPiece* now = RigOf(doll, piece);
  if (now == nullptr) {
    return std::unexpected(NoPiece(doll, piece));
  }
  const bool is_on = !now->joint_circle.is_on;
  return history_->Apply(
      Tr("Joint circle %1").arg(piece),
      WithRig(history_->current(), doll, [&](Rig* rig) {
        FindRig(rig, piece)->joint_circle.is_on = is_on;
        return Result<void>();
      }));
}

Result<void> RigManager::SetJointCircleColor(const QString& doll,
                                             const QString& piece,
                                             const QString& drawing,
                                             const QColor& color) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const RigPiece* now = RigOf(doll, piece);
  if (now == nullptr) {
    return std::unexpected(NoPiece(doll, piece));
  }
  const bool is_on = now->joint_circle.is_on;
  if (!is_on) {
    return std::unexpected(Error{
        Tr("Turn on the joint circle of %1 first (J).").arg(piece)});
  }
  if (drawing.isEmpty()) {
    return std::unexpected(
        Error{Tr("%1 shows no drawing to color the circle for.").arg(piece)});
  }
  const auto& colors = now->joint_circle.drawing_colors;
  const bool is_auto = !color.isValid();
  const bool has_own = colors.contains(drawing);
  if (is_auto && !has_own) {
    return std::unexpected(Error{
        Tr("The circle of %1 already takes %2's color.").arg(piece, drawing)});
  }
  auto next = WithRig(history_->current(), doll, [&](Rig* rig) {
    auto& own = FindRig(rig, piece)->joint_circle.drawing_colors;
    if (is_auto) {
      own.erase(drawing);
    } else {
      own[drawing] = color;
    }
    return Result<void>();
  });
  const QString label = is_auto ? Tr("Joint circle color auto %1").arg(piece)
                                : Tr("Joint circle color %1").arg(piece);
  return history_->Apply(label, std::move(next));
}

}  // namespace snapper
