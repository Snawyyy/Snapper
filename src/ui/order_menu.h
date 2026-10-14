#ifndef SNAPPER_UI_ORDER_MENU_H_
#define SNAPPER_UI_ORDER_MENU_H_

#include <QAction>
#include <QList>
#include <QObject>
#include <QString>
#include <QWidget>

#include "ui/managers.h"

namespace snapper {

// The Edit menu's "Draw one place forward" (Ctrl+]) and "Draw one place
// back" (Ctrl+[): they key the picked pieces' or doll layers' draw
// order one place up or down at the playhead (PoseManager::StepOrder),
// working anywhere on the Pose tab; the keys sheet and Find a command
// list them from the menu. Each greys out, with the reason in its
// tooltip, when nothing can be stepped (stage is the Pose tab's stage:
// off that tab they say so).
class OrderMenu final : public QObject {
  Q_OBJECT

 public:
  OrderMenu(const Managers& managers, QWidget* stage);

  // Both actions, to put in a menu.
  QList<QAction*> actions() { return {&forward_, &back_}; }
  // Greys and explains them for the playhead, the pick and the tab now;
  // the window calls it when the tab changes.
  void Refresh();

 private:
  // Why nothing can be stepped; empty when something can.
  QString WhyNot() const;
  void Step(int by);

  Managers managers_;
  QWidget* stage_;
  QAction forward_;
  QAction back_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_ORDER_MENU_H_
