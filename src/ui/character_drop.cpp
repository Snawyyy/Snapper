#include "ui/character_drop.h"

#include <cassert>
#include <set>

#include "base/text.h"
#include "edit/character_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"

namespace snapper {

QString PutCharactersOnStage(const Managers& managers,
                             const QStringList& characters) {
  assert(managers.IsComplete());
  assert(characters.size() < 100000);
  const bool has_shot = managers.selection->shot().IsValid();
  if (!has_shot) {
    return Tr("Add a shot first.");
  }
  for (const QString& name : characters) {
    const bool is_in =
        FindCharacter(managers.history->current(), name) != nullptr;
    if (!is_in) {
      const auto imported = managers.characters->Import(name);
      if (!imported) {
        return imported.error().message;
      }
    }
  }
  const auto layers = managers.stage->AddCharacters(
      managers.selection->shot(), characters);
  const bool is_failed = !layers.has_value();
  if (is_failed) {
    return layers.error().message;
  }
  std::set<Pick> picks;
  for (const LayerId layer : *layers) {
    picks.insert(Pick{layer});
  }
  managers.selection->PickThings(
      picks, PickMode::kReplace,
      layers->empty() ? LayerId() : layers->front());
  return QString();
}

}  // namespace snapper
