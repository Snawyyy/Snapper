// RigManager's drag nodes and physics nodes.

#include <algorithm>
#include <cassert>
#include <cmath>

#include "edit/history_manager.h"
#include "edit/rig_edits.h"
#include "edit/rig_manager.h"

namespace snapper {

Result<void> RigManager::ToggleDragNode(const QString& doll,
                                        const QString& piece, int point) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const Doll* found = FindDoll(history_->current(), doll);
  const RigPiece* rig = found != nullptr ? FindRig(found->rig, piece)
                                         : nullptr;
  const bool is_drag =
      rig != nullptr &&
      std::any_of(rig->drag_nodes.begin(), rig->drag_nodes.end(),
                  [point](const DragNode& n) { return n.point == point; });
  return history_->Apply(
      is_drag ? Tr("Stop dragging point of %1").arg(piece)
              : Tr("Drag point of %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [point, is_drag](RigPiece* edited) -> Result<void> {
                  const bool is_point =
                      point >= 0 && point < edited->warp.PointCount();
                  if (!is_point) {
                    return std::unexpected(Error{
                        Tr("Give the piece a warp grid in the rig first.")});
                  }
                  if (is_drag) {
                    std::erase_if(edited->drag_nodes,
                                  [point](const DragNode& n) {
                                    return n.point == point;
                                  });
                  } else {
                    edited->drag_nodes.push_back(DragNode{point});
                  }
                  return {};
                }));
}

namespace {

// The drag node of rig on point, or nullptr.
const DragNode* FindDrag(const RigPiece* rig, int point) {
  assert(point >= -1);
  assert(rig == nullptr || rig->drag_nodes.size() <= 4096);
  const bool is_rig = rig != nullptr;
  if (!is_rig) {
    return nullptr;
  }
  const auto found = std::find_if(
      rig->drag_nodes.begin(), rig->drag_nodes.end(),
      [point](const DragNode& n) { return n.point == point; });
  const bool is_drag = found != rig->drag_nodes.end();
  return is_drag ? &*found : nullptr;
}

}  // namespace

Result<void> RigManager::SetDrag(const QString& doll, const QString& piece,
                                 const DragNode& node) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const bool is_number =
      std::isfinite(node.lag) && std::isfinite(node.bounce) &&
      std::isfinite(node.stretch) && std::isfinite(node.inertia) &&
      std::isfinite(node.air) && std::isfinite(node.gravity) &&
      std::isfinite(node.spring);
  if (!is_number) {
    return std::unexpected(Error{Tr("That value is off the map.")});
  }
  const Doll* found_doll = FindDoll(history_->current(), doll);
  const DragNode* now =
      found_doll != nullptr
          ? FindDrag(FindRig(found_doll->rig, piece), node.point)
          : nullptr;
  const bool is_physics = now != nullptr && now->physics;
  return history_->Apply(
      is_physics ? Tr("Physics of %1").arg(piece) : Tr("Drag of %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [&node](RigPiece* edited) -> Result<void> {
                  const auto drag = std::find_if(
                      edited->drag_nodes.begin(), edited->drag_nodes.end(),
                      [&node](const DragNode& n) {
                        return n.point == node.point;
                      });
                  const bool is_drag = drag != edited->drag_nodes.end();
                  if (!is_drag) {
                    return std::unexpected(Error{
                        Tr("Press D on a picked dot to make it drag first.")});
                  }
                  const auto unit = [](double v) {
                    return std::clamp(v, 0.0, 1.0);
                  };
                  drag->lag = unit(node.lag);
                  drag->bounce = unit(node.bounce);
                  drag->stretch = unit(node.stretch);
                  drag->inertia = unit(node.inertia);
                  drag->air = unit(node.air);
                  drag->gravity = unit(node.gravity);
                  drag->spring = unit(node.spring);
                  return {};
                }));
}

Result<void> RigManager::SetPhysics(const QString& doll,
                                    const QString& piece, int point,
                                    bool on) {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const Doll* found_doll = FindDoll(history_->current(), doll);
  const DragNode* now =
      found_doll != nullptr
          ? FindDrag(FindRig(found_doll->rig, piece), point)
          : nullptr;
  const bool is_drag = now != nullptr;
  if (!is_drag) {
    return std::unexpected(
        Error{Tr("Press D on a picked dot to make it drag first.")});
  }
  const bool is_same = now->physics == on;
  if (is_same) {
    return {};
  }
  return history_->Apply(
      on ? Tr("Physics on for %1").arg(piece)
         : Tr("Physics off for %1").arg(piece),
      WithPiece(history_->current(), doll, piece,
                [point, on](RigPiece* edited) -> Result<void> {
                  for (DragNode& n : edited->drag_nodes) {
                    const bool is_point = n.point == point;
                    if (is_point) {
                      n.physics = on;
                    }
                  }
                  return {};
                }));
}

}  // namespace snapper
