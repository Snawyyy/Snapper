#include "ui/storyboard_actions.h"

#include <QAction>

#include <cassert>
#include <memory>

#include "edit/draw_manager.h"
#include "edit/storyboard_manager.h"
#include "ui/playhead.h"
#include "ui/problem.h"
#include "ui/toast_manager.h"

namespace snapper {
namespace {

// The action for a new frame (is_over false) or for drawing over the
// frame showing, owned by parent and calling RequestStoryboard.
std::unique_ptr<QAction> MakeAction(QObject* parent,
                                    const Managers& managers, bool is_over) {
  assert(parent != nullptr);
  assert(managers.storyboard != nullptr);
  auto action = std::make_unique<QAction>(
      is_over ? QObject::tr("Draw over storyboard frame")
              : QObject::tr("New storyboard frame here"),
      parent);
  action->setShortcut(is_over ? QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_N)
                              : QKeySequence(Qt::ALT | Qt::Key_N));
  QObject::connect(action.get(), &QAction::triggered, parent,
                   [managers, is_over] {
                     RequestStoryboard(managers, is_over);
                   });
  return action;
}

bool IsAvailable(const Managers& managers) {
  assert(managers.history != nullptr);
  assert(managers.toasts != nullptr);
  return managers.storyboard != nullptr && managers.draw != nullptr;
}

}  // namespace

QString WhyNoStoryboardHere(const Managers& managers, bool is_over) {
  assert(managers.history != nullptr);
  assert(managers.playback != nullptr);
  const bool is_available = IsAvailable(managers);
  if (!is_available) {
    return QObject::tr("Storyboards aren't available here.");
  }
  const auto spot = SpotOf(managers);
  if (!spot) {
    return QObject::tr("Put the playhead in a shot first.");
  }
  return managers.storyboard->WhyNoDraw(spot->shot, spot->local, is_over);
}

void RequestStoryboard(const Managers& managers, bool is_over) {
  assert(managers.toasts != nullptr);
  assert(managers.history != nullptr);
  const QString why_not = WhyNoStoryboardHere(managers, is_over);
  const bool is_refused = !why_not.isEmpty();
  if (is_refused) {
    managers.toasts->Problem(why_not);
    return;
  }
  const auto spot = SpotOf(managers);
  const Result<void> started =
      is_over ? managers.storyboard->DrawOver(spot->shot, spot->local)
              : managers.storyboard->DrawNew(spot->shot, spot->local);
  const QString problem = ProblemOf(started);
  const bool has_problem = !problem.isEmpty();
  if (has_problem) {
    managers.toasts->Problem(problem);
  }
}

void AddStoryboardActions(QMenu* menu, const Managers& managers) {
  assert(menu != nullptr);
  assert(managers.history != nullptr);
  const bool is_available = IsAvailable(managers);
  if (!is_available) {
    return;
  }
  for (const bool is_over : {false, true}) {
    QAction* action = MakeAction(menu, managers, is_over).release();
    menu->addAction(action);
    const QString why_not = WhyNoStoryboardHere(managers, is_over);
    const bool can_act = why_not.isEmpty();
    action->setEnabled(can_act);
    const QString help =
        is_over ? QObject::tr("Draw on the storyboard frame showing at the "
                              "playhead.")
                : QObject::tr("A blank frame from the playhead on, with the "
                              "frame before faint to trace from.");
    action->setToolTip(can_act ? help : why_not);
  }
}

}  // namespace snapper
