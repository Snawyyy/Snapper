#include "ui/reference_keys.h"

#include <cassert>
#include <set>

#include "edit/reference_manager.h"
#include "edit/selection_manager.h"
#include "ui/playhead.h"
#include "ui/problem.h"

namespace snapper {

bool ReferenceKey(const QKeyEvent& event, const Managers& managers,
                  QString* problem) {
  assert(problem != nullptr);
  assert(managers.selection != nullptr);
  problem->clear();
  const bool has_manager = managers.references != nullptr;
  if (!has_manager) {
    return false;
  }
  // With references picked, the pick keys act on them: Ctrl+A picks
  // every reference row, Delete removes the picked ones, Escape lets
  // them go.
  const ShotId here = managers.selection->shot();
  const bool has_picks = !managers.references->PicksIn(here).empty();
  const bool is_all = has_picks && event.matches(QKeySequence::SelectAll);
  if (is_all) {
    std::set<ReferenceId> all;
    for (const Reference& reference : managers.references->Of(here)) {
      all.insert(reference.id);
    }
    managers.references->PickMany(here, all, PickMode::kReplace,
                                  managers.references->PickedIn(here));
    return true;
  }
  const bool is_delete = has_picks && (event.key() == Qt::Key_Delete ||
                                       event.key() == Qt::Key_Backspace);
  if (is_delete) {
    *problem = ProblemOf(managers.references->RemovePicked(here));
    return true;
  }
  const bool is_escape = has_picks && event.key() == Qt::Key_Escape;
  if (is_escape) {
    managers.references->Pick(here, ReferenceId());
    return true;
  }
  const bool is_plain = event.modifiers() == Qt::NoModifier;
  if (!is_plain || event.isAutoRepeat()) {
    return false;
  }
  const bool is_toggle = event.key() == Qt::Key_Y;
  if (is_toggle) {
    managers.references->ToggleShown();
    return true;
  }
  const ShotId shot = managers.selection->shot();
  const ReferenceId picked = managers.references->PickedIn(shot);
  const bool is_mark = event.key() == Qt::Key_M && picked.IsValid();
  if (!is_mark) {
    return false;
  }
  const auto spot = SpotOf(managers);
  const bool is_here = spot.has_value() && spot->shot == shot;
  *problem = is_here ? ProblemOf(managers.references->AddMark(
                           shot, picked, spot->local))
                     : QObject::tr("Move the playhead into the shot first.");
  return true;
}

}  // namespace snapper
