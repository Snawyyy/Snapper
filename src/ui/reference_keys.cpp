#include "ui/reference_keys.h"

#include <cassert>

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
  const bool is_plain = event.modifiers() == Qt::NoModifier;
  if (!has_manager || !is_plain || event.isAutoRepeat()) {
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
