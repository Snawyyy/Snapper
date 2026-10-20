// CharacterManager's shaping edits: cutting lines and the magnet.

#include <cassert>

#include "edit/character_manager_inl.h"
#include "edit/vector_reshape.h"

namespace snapper {

using character_manager_detail::OnDrawing;

Result<void> CharacterManager::DeleteEdge(const QString& character,
                                          PartId part, VectorEdge edge) {
  assert(history_ != nullptr);
  assert(part.value() >= 0);
  return Edit(character, Tr("Delete edge"), [&](Character* edited) {
    return OnDrawing(edited, part, [&](VectorDrawing* drawing, int*) {
      return snapper::DeleteEdge(drawing, edge);
    });
  });
}

Result<void> CharacterManager::ShiftPoints(
    const QString& character, PartId part,
    std::span<const std::pair<PointId, QPointF>> moves) {
  assert(history_ != nullptr);
  assert(moves.size() <= static_cast<size_t>(kMaxVectorPoints));
  return Edit(character, Tr("Magnet"), [&](Character* edited) {
    return OnDrawing(edited, part, [&](VectorDrawing* drawing, int*) {
      return snapper::ShiftPoints(drawing, moves);
    });
  });
}

}  // namespace snapper
