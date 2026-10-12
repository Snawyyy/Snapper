#include "ui/hold_actions.h"

#include <cassert>
#include <vector>

#include "edit/link_manager.h"
#include "edit/object_manager.h"
#include "edit/selection_manager.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"
#include "ui/toast_manager.h"

namespace snapper {
namespace {

// Toasts what went wrong, if anything.
void Tell(const Managers& managers, const QString& problem) {
  assert(managers.toasts != nullptr);
  const bool is_problem = !problem.isEmpty();
  if (is_problem) {
    managers.toasts->Problem(problem);
  }
}

}  // namespace

void AddHoldActions(QMenu* menu, const Managers& managers, ShotId shot,
                    Frame frame, const std::optional<LinkEnd>& hit) {
  assert(menu != nullptr);
  assert(frame.index() >= 0);
  ObjectManager* objects = managers.objects;
  const bool is_usable = objects != nullptr &&
                         managers.selection->shot() == shot;
  if (!is_usable) {
    return;
  }
  const std::vector<LayerId> picked =
      objects->ObjectLayers(shot, managers.selection->PickedLayers());
  const bool has_objects = !picked.empty();
  if (!has_objects) {
    return;
  }
  const bool is_piece = hit.has_value() && !hit->piece.isEmpty() &&
                        !objects->IsObjectLayer(shot, hit->layer);
  if (is_piece) {
    QAction* hold = menu->addAction(
        QObject::tr("Hold with %1").arg(managers.links->NameOf(shot, *hit)));
    const QString why_not = objects->WhyNoHold(shot, picked, *hit);
    Explain(hold, why_not);
    if (why_not.isEmpty()) {
      hold->setToolTip(QObject::tr("From this frame the picked objects "
                                   "follow it: place, turn and size."));
    }
    QObject::connect(hold, &QAction::triggered, menu,
                     [managers, shot, picked, holder = *hit, frame] {
                       Tell(managers,
                            ProblemOf(managers.objects->Hold(
                                shot, picked, holder, frame)));
                     });
  }
  const QString no_drop = objects->WhyNoDrop(shot, picked, frame);
  const bool can_drop = no_drop.isEmpty();
  if (can_drop) {
    QAction* drop = menu->addAction(QObject::tr("Drop here"));
    drop->setToolTip(QObject::tr("Let go at this frame; it stays where it "
                                 "is."));
    QObject::connect(drop, &QAction::triggered, menu,
                     [managers, shot, picked, frame] {
                       Tell(managers, ProblemOf(managers.objects->Drop(
                                          shot, picked, frame)));
                     });
  }
  const QString no_remove = objects->WhyNoUndoChange(shot, picked, frame);
  const bool can_remove = no_remove.isEmpty();
  if (can_remove) {
    QAction* remove = menu->addAction(QObject::tr("Remove hold change"));
    remove->setToolTip(QObject::tr("Whatever held it before goes on "
                                   "holding it."));
    QObject::connect(remove, &QAction::triggered, menu,
                     [managers, shot, picked, frame] {
                       Tell(managers, ProblemOf(managers.objects->RemoveChange(
                                          shot, picked, frame)));
                     });
  }
}

}  // namespace snapper
