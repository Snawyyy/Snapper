// CharacterManager's shaping edits (cutting lines, the magnet) and its
// named styles.

#include <cassert>

#include "edit/character_manager_inl.h"
#include "edit/character_styles.h"
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

Result<StyleId> CharacterManager::AddStyle(const QString& character,
                                           const QString& name,
                                           const VectorShape& look) {
  assert(history_ != nullptr);
  assert(name.size() < 100000);
  StyleId made;
  const auto done =
      Edit(character, Tr("New style"), [&](Character* edited) -> Result<void> {
        const Result<StyleId> id = snapper::AddStyle(edited, name, look);
        if (!id) {
          return std::unexpected(id.error());
        }
        made = *id;
        return {};
      });
  if (!done) {
    return std::unexpected(done.error());
  }
  return made;
}

Result<void> CharacterManager::RenameStyle(const QString& character,
                                           StyleId id, const QString& name) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  return Edit(character, Tr("Rename style"), [&](Character* edited) {
    return snapper::RenameStyle(edited, id, name);
  });
}

Result<void> CharacterManager::RemoveStyle(const QString& character,
                                           StyleId id) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  return Edit(character, Tr("Delete style"), [&](Character* edited) {
    return snapper::RemoveStyle(edited, id);
  });
}

Result<void> CharacterManager::UseStyle(const QString& character, PartId part,
                                        int shape, StyleId id) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  return Edit(character, id.IsValid() ? Tr("Use style") : Tr("Own look"),
              [&](Character* edited) {
                return snapper::UseStyle(edited, part, shape, id);
              });
}

Result<void> CharacterManager::SetShapeLook(const QString& character,
                                            PartId part, int shape,
                                            const VectorShape& look) {
  assert(history_ != nullptr);
  assert(look.stroke_width >= 0.0);
  return Edit(character, Tr("Shape look"), [&](Character* edited) {
    return snapper::SetShapeLook(edited, part, shape, look);
  });
}

Result<void> CharacterManager::SetStyleLook(const QString& character,
                                            StyleId id,
                                            const VectorShape& look) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  return Edit(character, Tr("Style look"), [&](Character* edited) {
    return snapper::SetStyleLook(edited, id, look);
  });
}

}  // namespace snapper
