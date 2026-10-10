// RigManager's edits to how moving dots move: their kind (drag, cloth,
// jelly) and their feel. Each is one undo step named with how many dots
// it touched.

#include <cassert>
#include <cmath>

#include "anim/node_feel.h"
#include "edit/history_manager.h"
#include "edit/rig_edits.h"
#include "edit/rig_manager.h"

namespace snapper {

Result<void> RigManager::SetNodeKindAll(const std::vector<DotRef>& dots,
                                        NodeKind kind) {
  assert(history_ != nullptr);
  assert(static_cast<int>(kind) >= 0 &&
         static_cast<int>(kind) < kNodeKindCount);
  const bool has_dots = !dots.empty();
  if (!has_dots) {
    return NeedDots();
  }
  const QString names[] = {Tr("Drag %1"), Tr("Cloth %1"), Tr("Jelly %1")};
  return history_->Apply(
      names[static_cast<size_t>(kind)].arg(CountText(dots.size())),
      WithDots(history_->current(), dots,
               [kind](RigPiece* rig, int point) -> Result<void> {
                 DragNode* node = FindNode(rig, point);
                 const bool is_new = node == nullptr;
                 if (is_new) {
                   rig->drag_nodes.push_back(DragNode{point});
                   node = &rig->drag_nodes.back();
                 }
                 node->kind = kind;
                 return {};
               }));
}

QString RigManager::FeelLabel(size_t dots) {
  assert(dots > 0);
  assert(dots < 10000000);
  return Tr("Set feel on %1").arg(CountText(dots));
}

Result<void> RigManager::SetFeelAll(const std::vector<DotRef>& dots,
                                    const Feel& feel) {
  assert(history_ != nullptr);
  assert(dots.size() < 10000000);
  const bool is_number = std::isfinite(feel.firm) && std::isfinite(feel.heavy);
  if (!is_number) {
    return std::unexpected(Error{Tr("That feel is off the map.")});
  }
  const bool has_dots = !dots.empty();
  if (!has_dots) {
    return NeedDots();
  }
  return history_->Apply(
      FeelLabel(dots.size()),
      WithDots(history_->current(), dots,
               [&feel](RigPiece* rig, int point) -> Result<void> {
                 DragNode* node = FindNode(rig, point);
                 const bool is_node = node != nullptr;
                 if (is_node) {
                   *node = WithFeel(*node, feel);
                 }
                 return {};
               }));
}

}  // namespace snapper
