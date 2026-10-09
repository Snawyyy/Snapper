#ifndef SNAPPER_UI_SPACE_KEYS_ACTION_H_
#define SNAPPER_UI_SPACE_KEYS_ACTION_H_

#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QWidget>

#include <array>

#include "ui/managers.h"

namespace snapper {

// Space evenly offers on 1s up to on this many, each on its digit key.
constexpr int kSpacings = 4;

// The digit key that spaces picked keys on step frames (1 to
// kSpacings); empty for any other step.
QKeySequence SpaceShortcut(int step);

// The step a pressed key stands for, or 0 when it is not a digit
// from 1 to kSpacings.
int SpaceStepOfKey(int key);

// Spaces the picked keys on step frames, keeps them picked, or toasts
// why it can't.
void RequestSpaceKeys(const Managers& managers, int step);

// Adds "Space evenly" to menu with "On Ns" for each spacing, showing
// its digit. The submenu is greyed with the reason as its tooltip when
// it can't act.
void AddSpaceKeysMenu(QMenu* menu, const Managers& managers);

// The 1 to 4 keys, live anywhere in the window. A digit typed into a
// text or number field still types: those fields claim it first.
class SpaceKeysShortcuts final {
 public:
  SpaceKeysShortcuts(QWidget* window, const Managers& managers);

  const std::array<QAction, kSpacings>& actions() const { return actions_; }

 private:
  std::array<QAction, kSpacings> actions_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SPACE_KEYS_ACTION_H_
