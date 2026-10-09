// RigManager's edits to many warp dots at once: drag nodes, physics
// nodes, point motions. Each is one undo step named with how many dots
// it touched.

#include <algorithm>
#include <cassert>
#include <cmath>

#include "edit/history_manager.h"
#include "edit/rig_edits.h"
#include "edit/rig_manager.h"

namespace snapper {
namespace {

QString CountText(size_t count) {
  assert(count > 0);
  assert(count < 10000000);
  const bool is_one = count == 1;
  return is_one ? Tr("1 dot") : Tr("%1 dots").arg(count);
}

QString FieldName(DragField field) {
  assert(static_cast<int>(field) >= 0);
  assert(static_cast<int>(field) <= static_cast<int>(DragField::kSpring));
  switch (field) {
    case DragField::kLag:
      return Tr("lag");
    case DragField::kBounce:
    case DragField::kSpring:
      return Tr("bounce");
    case DragField::kStretch:
      return Tr("stretch");
    case DragField::kInertia:
      return Tr("inertia");
    case DragField::kAir:
      return Tr("air resistance");
    case DragField::kGravity:
      return Tr("gravity");
  }
  return QString();
}

double* FieldOf(DragNode* node, DragField field) {
  assert(node != nullptr);
  assert(static_cast<int>(field) >= 0);
  switch (field) {
    case DragField::kLag:
      return &node->lag;
    case DragField::kBounce:
      return &node->bounce;
    case DragField::kStretch:
      return &node->stretch;
    case DragField::kInertia:
      return &node->inertia;
    case DragField::kAir:
      return &node->air;
    case DragField::kGravity:
      return &node->gravity;
    case DragField::kSpring:
      return &node->spring;
  }
  return &node->lag;
}

DragNode* FindNode(RigPiece* rig, int point) {
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

bool IsDragNode(const Project& project, const DotRef& dot) {
  assert(dot.point >= 0);
  assert(!dot.piece.isEmpty());
  const Doll* doll = FindDoll(project, dot.doll);
  const RigPiece* rig =
      doll != nullptr ? FindRig(doll->rig, dot.piece) : nullptr;
  const bool has_rig = rig != nullptr;
  return has_rig && std::any_of(rig->drag_nodes.begin(),
                                rig->drag_nodes.end(),
                                [&dot](const DragNode& n) {
                                  return n.point == dot.point;
                                });
}

// The dots of dots that are drag nodes now.
std::vector<DotRef> DragNodesOf(const Project& project,
                                const std::vector<DotRef>& dots) {
  assert(dots.size() < 10000000);
  assert(project.dolls.size() < 10000000);
  std::vector<DotRef> nodes;
  for (const DotRef& dot : dots) {
    const bool is_node = IsDragNode(project, dot);
    if (is_node) {
      nodes.push_back(dot);
    }
  }
  return nodes;
}

// change(rig, point) on each of dots; the first refusal stops it.
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
                       return change(rig, dot.point);
                     });
    const bool is_refused = !next;
    if (is_refused) {
      return next;
    }
  }
  return next;
}

Result<void> NeedNodes() {
  assert(kMaxWarpPoints > 0);
  assert(kMaxIkChains > 0);
  return std::unexpected(
      Error{Tr("Press D on a picked dot to make it drag first.")});
}

Result<void> NeedDots() {
  assert(kMaxWarpPoints > 0);
  assert(kMaxIkChains > 0);
  return std::unexpected(Error{Tr("Pick one or more warp dots first.")});
}

// Sets one field of the motion of point, starting one when it has none.
void ChangeMotion(RigPiece* rig, int point, MotionField field, double value) {
  assert(rig != nullptr);
  assert(point >= 0);
  const PointMotion* old = FindMotion(*rig, point);
  PointMotion motion;
  motion.point = point;
  const bool has_old = old != nullptr;
  if (has_old) {
    motion = *old;
  }
  switch (field) {
    case MotionField::kKind:
      motion.kind = static_cast<WarpMotionKind>(std::lround(value));
      break;
    case MotionField::kSize:
      motion.size = value;
      break;
    case MotionField::kCycle:
      motion.cycle = static_cast<int>(std::lround(value));
      break;
    case MotionField::kAngle:
      motion.angle = value;
      break;
    case MotionField::kDelay:
      motion.delay = value;
      break;
  }
  std::erase_if(rig->point_motions, [point](const PointMotion& m) {
    return m.point == point;
  });
  const bool is_moving = motion.kind != WarpMotionKind::kNone;
  if (is_moving) {
    rig->point_motions.push_back(KeptMotion(motion));
  }
}

}  // namespace

QString RigManager::FieldLabel(DragField field, size_t dots) {
  assert(dots > 0);
  assert(static_cast<int>(field) >= 0);
  return Tr("Set %1 on %2").arg(FieldName(field), CountText(dots));
}

Result<void> RigManager::SetDragField(const std::vector<DotRef>& dots,
                                      DragField field, double value) {
  assert(history_ != nullptr);
  assert(static_cast<int>(field) >= 0);
  const bool is_number = std::isfinite(value);
  if (!is_number) {
    return std::unexpected(Error{Tr("That value is off the map.")});
  }
  const std::vector<DotRef> nodes = DragNodesOf(history_->current(), dots);
  const bool has_nodes = !nodes.empty();
  if (!has_nodes) {
    return NeedNodes();
  }
  const double kept = std::clamp(value, 0.0, 1.0);
  return history_->Apply(
      FieldLabel(field, nodes.size()),
      WithDots(history_->current(), nodes,
               [field, kept](RigPiece* rig, int point) -> Result<void> {
                 DragNode* node = FindNode(rig, point);
                 const bool is_node = node != nullptr;
                 if (is_node) {
                   *FieldOf(node, field) = kept;
                 }
                 return {};
               }));
}

Result<void> RigManager::SetDragNodes(const std::vector<DotRef>& dots,
                                      bool on) {
  assert(history_ != nullptr);
  assert(dots.size() < 10000000);
  const bool has_dots = !dots.empty();
  if (!has_dots) {
    return NeedDots();
  }
  return history_->Apply(
      on ? Tr("Drag %1").arg(CountText(dots.size()))
         : Tr("Stop dragging %1").arg(CountText(dots.size())),
      WithDots(history_->current(), dots,
               [on](RigPiece* rig, int point) -> Result<void> {
                 const bool is_there = FindNode(rig, point) != nullptr;
                 const bool is_adding = on && !is_there;
                 if (is_adding) {
                   rig->drag_nodes.push_back(DragNode{point});
                 }
                 const bool is_removing = !on;
                 if (is_removing) {
                   std::erase_if(rig->drag_nodes, [point](const DragNode& n) {
                     return n.point == point;
                   });
                 }
                 return {};
               }));
}

Result<void> RigManager::SetPhysicsAll(const std::vector<DotRef>& dots,
                                       bool on) {
  assert(history_ != nullptr);
  assert(dots.size() < 10000000);
  const std::vector<DotRef> nodes = DragNodesOf(history_->current(), dots);
  const bool has_nodes = !nodes.empty();
  if (!has_nodes) {
    return NeedNodes();
  }
  return history_->Apply(
      on ? Tr("Physics on for %1").arg(CountText(nodes.size()))
         : Tr("Physics off for %1").arg(CountText(nodes.size())),
      WithDots(history_->current(), nodes,
               [on](RigPiece* rig, int point) -> Result<void> {
                 DragNode* node = FindNode(rig, point);
                 const bool is_node = node != nullptr;
                 if (is_node) {
                   node->physics = on;
                 }
                 return {};
               }));
}

Result<void> RigManager::ClearDots(const std::vector<DotRef>& dots) {
  assert(history_ != nullptr);
  assert(dots.size() < 10000000);
  const bool has_dots = !dots.empty();
  if (!has_dots) {
    return NeedDots();
  }
  return history_->Apply(
      Tr("Clear %1").arg(CountText(dots.size())),
      WithDots(history_->current(), dots,
               [](RigPiece* rig, int point) -> Result<void> {
                 std::erase_if(rig->drag_nodes, [point](const DragNode& n) {
                   return n.point == point;
                 });
                 std::erase_if(rig->point_motions,
                               [point](const PointMotion& m) {
                                 return m.point == point;
                               });
                 return {};
               }));
}

QString RigManager::MotionLabel(MotionField field, size_t dots) {
  assert(dots > 0);
  assert(static_cast<int>(field) >= 0);
  const QString names[] = {Tr("how it moves"), Tr("size"), Tr("cycle"),
                           Tr("direction"), Tr("delay")};
  return Tr("Set %1 on %2")
      .arg(names[static_cast<size_t>(field)], CountText(dots));
}

Result<void> RigManager::SetMotionAll(const std::vector<DotRef>& dots,
                                      MotionField field, double value) {
  assert(history_ != nullptr);
  assert(static_cast<int>(field) >= 0);
  const int kind = static_cast<int>(std::lround(value));
  const bool is_kind = field == MotionField::kKind;
  const bool is_kind_ok = kind >= 0 && kind < kWarpMotionKindCount;
  const bool is_usable = std::isfinite(value) && (!is_kind || is_kind_ok);
  if (!is_usable) {
    return std::unexpected(Error{Tr("That value is off the map.")});
  }
  const bool has_dots = !dots.empty();
  if (!has_dots) {
    return NeedDots();
  }
  return history_->Apply(
      MotionLabel(field, dots.size()),
      WithDots(history_->current(), dots,
               [field, value](RigPiece* rig, int point) -> Result<void> {
                 ChangeMotion(rig, point, field, value);
                 return {};
               }));
}

}  // namespace snapper
