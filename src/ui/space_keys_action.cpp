#include "ui/space_keys_action.h"

#include <QAction>

#include <cassert>

#include "edit/key_manager.h"
#include "edit/selection_manager.h"
#include "ui/problem.h"
#include "ui/toast_manager.h"

namespace snapper {

QKeySequence SpaceShortcut(int step) {
  assert(kSpacings >= 1);
  assert(Qt::Key_1 + kSpacings <= Qt::Key_9);
  const bool is_known = step >= 1 && step <= kSpacings;
  if (!is_known) {
    return QKeySequence();
  }
  return QKeySequence(Qt::Key_0 + step);
}

int SpaceStepOfKey(int key) {
  assert(kSpacings >= 1);
  assert(Qt::Key_0 < Qt::Key_1);
  const int step = key - Qt::Key_0;
  const bool is_known = step >= 1 && step <= kSpacings;
  return is_known ? step : 0;
}

void RequestSpaceKeys(const Managers& managers, int step) {
  assert(managers.keys != nullptr);
  assert(managers.selection != nullptr);
  assert(managers.toasts != nullptr);
  const bool is_known = step >= 1 && step <= kSpacings;
  if (!is_known) {
    return;
  }
  const auto spaced = managers.keys->Space(managers.selection->keys(), step);
  if (spaced) {
    managers.selection->SelectKeys(*spaced, false);
  }
  managers.toasts->Problem(ProblemOf(spaced));
}

void AddSpaceKeysMenu(QMenu* menu, const Managers& managers) {
  assert(menu != nullptr);
  assert(managers.keys != nullptr);
  QMenu& space = *menu->addMenu(QObject::tr("Space evenly"));
  const QString why_not =
      managers.keys->WhyNoSpace(managers.selection->keys());
  space.setEnabled(why_not.isEmpty());
  space.menuAction()->setToolTip(why_not);
  for (int step = 1; step <= kSpacings; ++step) {
    const QString hotkey =
        SpaceShortcut(step).toString(QKeySequence::NativeText);
    QAction* action = space.addAction(
        QObject::tr("On %1s\t%2").arg(step).arg(hotkey));
    QObject::connect(action, &QAction::triggered, menu,
                     [managers, step] { RequestSpaceKeys(managers, step); });
  }
}

SpaceKeysShortcuts::SpaceKeysShortcuts(QWidget* window,
                                       const Managers& managers) {
  assert(window != nullptr);
  assert(managers.keys != nullptr);
  for (int step = 1; step <= kSpacings; ++step) {
    QAction& action = actions_[static_cast<size_t>(step - 1)];
    action.setShortcut(SpaceShortcut(step));
    action.setText(QObject::tr("Space keys on %1s").arg(step));
    window->addAction(&action);
    QObject::connect(&action, &QAction::triggered, window,
                     [managers, step] { RequestSpaceKeys(managers, step); });
  }
}

}  // namespace snapper
