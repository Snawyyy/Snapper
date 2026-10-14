#ifndef SNAPPER_UI_STORYBOARD_MENU_H_
#define SNAPPER_UI_STORYBOARD_MENU_H_

#include <QAction>
#include <QList>
#include <QObject>
#include <QWidget>

#include "ui/managers.h"

namespace snapper {

// The Edit menu's "New storyboard frame here" (Alt+N) and "Draw over
// storyboard frame" (Alt+Shift+N): the keys work anywhere on the Pose
// tab, and the keys sheet and Find a command list them from the menu.
// Each greys out, with the reason in its tooltip, when it can't act
// (stage is the Pose tab's stage: off that tab they say so).
class StoryboardMenu final : public QObject {
  Q_OBJECT

 public:
  StoryboardMenu(const Managers& managers, QWidget* stage);

  // Both actions, to put in a menu.
  QList<QAction*> actions() { return {&new_, &over_}; }
  // Greys and explains them for the playhead, the pick and the tab now;
  // the window calls it when the tab changes.
  void Refresh();

 private:
  Managers managers_;
  QWidget* stage_;
  QAction new_;
  QAction over_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_STORYBOARD_MENU_H_
