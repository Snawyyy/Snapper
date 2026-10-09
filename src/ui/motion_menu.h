#ifndef SNAPPER_UI_MOTION_MENU_H_
#define SNAPPER_UI_MOTION_MENU_H_

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QObject>

#include "ui/motion_table.h"

namespace snapper {

class MotionManager;

// The View menu's Motion switch: Full, Calm, Off, one checked. A pick
// applies at once; a change made elsewhere moves the check.
class MotionMenu final : public QObject {
  Q_OBJECT

 public:
  explicit MotionMenu(MotionManager* motion);

  QMenu* menu() { return &menu_; }

 private:
  void Show(MotionMode mode);

  MotionManager* motion_;
  QMenu menu_;
  QActionGroup group_;
  QAction full_action_;
  QAction calm_action_;
  QAction off_action_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_MOTION_MENU_H_
