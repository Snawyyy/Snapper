#ifndef SNAPPER_UI_MAIN_WINDOW_H_
#define SNAPPER_UI_MAIN_WINDOW_H_

#include <QAction>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/cast_panel.h"
#include "ui/export_dialog.h"
#include "ui/file_menu.h"
#include "ui/inspector.h"
#include "ui/managers.h"
#include "ui/rig_canvas.h"
#include "ui/rig_panel.h"
#include "ui/shot_strip.h"
#include "ui/stage_view.h"
#include "ui/timeline_view.h"
#include "ui/video_page.h"

namespace snapper {

// The window frame: menus, the Pose, Rig and Video tabs, the status
// bar, and the panels docked around the stage. It shows manager state and
// forwards input; it decides nothing itself.
class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  enum class Mode { kPose = 0, kRig = 1, kVideo = 2 };

  explicit MainWindow(const Managers& managers);

  // Called once the window shows: offers to recover crashed work.
  void Start();
  Mode mode() const { return static_cast<Mode>(modes_.currentIndex()); }
  void SetMode(Mode mode) { modes_.setCurrentIndex(static_cast<int>(mode)); }

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void BuildMenus();
  // Pulls title, undo and redo state from the history.
  void Refresh();
  // Keeps the picked shot on the one under the playhead.
  void FollowPlayhead();
  // Shows the page for the picked tab and puts the playhead on its
  // timeline; the posing panels step aside on the Video tab.
  void ShowMode(int index);

  Managers managers_;
  FileMenu file_menu_;
  QMenu edit_menu_;
  QAction undo_action_;
  QAction redo_action_;
  QWidget center_;
  QVBoxLayout center_layout_;
  QTabBar modes_;
  QStackedWidget pages_;
  // Containers come before what they hold, so the held widgets leave
  // them before they go.
  QWidget pose_page_;
  QVBoxLayout pose_layout_;
  ShotStrip strip_;
  QSplitter pose_split_;
  StageView stage_;
  TimelineView timeline_;
  QSplitter rig_split_;
  RigCanvas rig_canvas_;
  RigPanel rig_panel_;
  VideoPage video_page_;
  QDockWidget cast_dock_;
  CastPanel cast_;
  QDockWidget inspector_dock_;
  Inspector inspector_;
  ExportDialog export_dialog_;
  QAction export_action_;
  QMenu play_menu_;
  QAction play_action_;
  QAction next_action_;
  QAction back_action_;
  // Whether the posing panels were open before the Video tab hid them.
  bool was_cast_open_ = true;
  bool was_inspector_open_ = true;
};

}  // namespace snapper

#endif  // SNAPPER_UI_MAIN_WINDOW_H_
