#include "ui/doll_drop.h"

#include <cassert>
#include <set>

#include "base/text.h"
#include "edit/character_manager.h"
#include "edit/doll_library_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"

namespace snapper {

QString PutDollsOnStage(const Managers& managers, const QStringList& dolls) {
  assert(managers.IsComplete());
  assert(dolls.size() < 100000);
  const bool has_shot = managers.selection->shot().IsValid();
  if (!has_shot) {
    return Tr("Add a shot first.");
  }
  QStringList missing;
  const QStringList characters =
      managers.characters != nullptr ? managers.characters->Available()
                                     : QStringList();
  for (const QString& name : dolls) {
    const Project& project = managers.history->current();
    const bool is_in = FindDoll(project, name) != nullptr ||
                       FindCharacter(project, name) != nullptr;
    const bool is_character = characters.contains(name) &&
                              !managers.library->Available().contains(name);
    if (!is_in && is_character) {
      const auto imported = managers.characters->Import(name);
      if (!imported) {
        return imported.error().message;
      }
    } else if (!is_in) {
      missing.append(name);
    }
  }
  const bool needs_import = !missing.isEmpty();
  if (needs_import) {
    const auto imported = managers.library->ImportAll(missing);
    const bool is_failed = !imported.has_value();
    if (is_failed) {
      return imported.error().message;
    }
  }
  const auto layers = managers.stage->AddDolls(managers.selection->shot(),
                                               dolls);
  const bool is_failed = !layers.has_value();
  if (is_failed) {
    return layers.error().message;
  }
  std::set<Pick> picks;
  for (const LayerId layer : *layers) {
    picks.insert({layer, QString()});
  }
  managers.selection->PickThings(
      picks, PickMode::kReplace,
      layers->empty() ? LayerId() : layers->front());
  return QString();
}

}  // namespace snapper
