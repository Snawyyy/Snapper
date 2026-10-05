#ifndef SNAPPER_UI_MAIN_WINDOW_H_
#define SNAPPER_UI_MAIN_WINDOW_H_

#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/file_menu.h"
#include "ui/managers.h"

namespace snapper {

// The window frame: menus, the Pose and Rig tabs, the status bar, and
// the panels docked around the stage. It shows manager state and
// forwards input; it decides nothing itself.
class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  enum class Mode { kPose = 0, kRig = 1 };

  explicit MainWindow(const Managers& managers);

  // Called once the window shows: offers to recover crashed work.
  void Start();
  Mode mode() const { return static_cast<Mode>(modes_.currentIndex()); }

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void BuildMenus();
  // Pulls title, undo and redo state from the history.
  void Refresh();

  Managers managers_;
  FileMenu file_menu_;
  QMenu edit_menu_;
  QAction undo_action_;
  QAction redo_action_;
  QWidget center_;
  QVBoxLayout center_layout_;
  QTabBar modes_;
  QStackedWidget pages_;
  // Shown on a mode until its page arrives.
  QLabel pose_empty_;
  QLabel rig_empty_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_MAIN_WINDOW_H_
