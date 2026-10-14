#ifndef SNAPPER_UI_MAIN_WINDOW_H_
#define SNAPPER_UI_MAIN_WINDOW_H_

#include <QAction>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/dock_grips.h"
#include "ui/grip_splitter.h"
#include "ui/cast_panel.h"
#include "ui/krita_menu.h"
#include "ui/palette_panel.h"
#include "ui/command_palette.h"
#include "ui/comp_page.h"
#include "ui/export_dialog.h"
#include "ui/file_menu.h"
#include "ui/inspector.h"
#include "ui/managers.h"
#include "ui/motion_menu.h"
#include "ui/refusal_shaker.h"
#include "ui/rig_canvas.h"
#include "ui/rig_panel.h"
#include "ui/shortcut_sheet.h"
#include "ui/settings_actions.h"
#include "ui/order_menu.h"
#include "ui/storyboard_menu.h"
#include "ui/shot_strip.h"
#include "ui/success_burst.h"
#include "ui/space_keys_action.h"
#include "ui/stage_view.h"
#include "ui/top_bar.h"
#include "ui/timeline_view.h"
#include "ui/transport_bar.h"
#include "ui/timing_chart_dialog.h"
#include "ui/toast_view.h"
#include "ui/ui_sound.h"
#include "ui/video_page.h"

namespace snapper {

// The window frame: menus, the Pose, Rig, Comp and Video tabs, the status
// bar, and the panels docked around the stage. It shows manager state and
// forwards input; it decides nothing itself.
class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  // In the order of the workflow in docs/redesign/ux-polish-plan.md: plan
  // the reel, animate the shots, finish them; Rig, visited least, is
  // last.
  enum class Mode { kVideo = 0, kPose = 1, kComp = 2, kRig = 3 };

  explicit MainWindow(const Managers& managers);

  // Called once the window shows: offers to recover crashed work.
  // Opens path when it is not empty, else offers to recover a crash.
  void Start(const QString& path = QString());
  Mode mode() const {
    return static_cast<Mode>(top_bar_.current_tab());
  }
  void SetMode(Mode mode) {
    top_bar_.tabs()->setCurrentIndex(static_cast<int>(mode));
  }

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void BuildMenus();
  // The View menu's Sounds switch and the Help menu.
  void BuildViewAndHelp();
  // Puts the Pose, Rig, Comp and Video pages in the stack.
  void BuildPages();
  // Puts the top bar in its fixed host and ties the tabs to the pages.
  void BuildModes();
  void BuildHiddenFoldersAction();
  // Pulls title, undo and redo state from the history.
  void Refresh();
  // Undo or redo in the shot being worked in; the newest change when no
  // shot is (the Video tab).
  void UndoStep();
  void RedoStep();
  ShotId UndoShot() const;
  // Keeps the picked shot on the one under the playhead.
  void FollowPlayhead();
  // Shows the page for the picked tab and puts the playhead on its
  // timeline; the posing panels step aside on the Video tab.
  void ShowMode(int index);

  Managers managers_;
  FileMenu file_menu_;
  KritaMenu krita_menu_;
  // The floating color palette and the View menu entry that opens it.
  PalettePanel color_palette_;
  QAction color_palette_action_;
  QMenu edit_menu_;
  QMenu view_menu_;
  MotionMenu motion_menu_;
  // Quiet sounds for undo, redo and big wins; the View menu turns them
  // off.
  UiSound sound_;
  QAction sounds_action_;
  // Shows the library folders that are marked hidden.
  QAction hidden_folders_action_;
  QAction undo_action_;
  QAction redo_action_;
  // The bar sits fixed across the top, above the docks, so hiding the
  // docks on the Video tab does not slide the tabs sideways.
  QToolBar top_bar_host_;
  TopBar top_bar_;
  QStackedWidget pages_;
  // Containers come before what they hold, so the held widgets leave
  // them before they go.
  QWidget pose_page_;
  QVBoxLayout pose_layout_;
  ShotStrip strip_;
  GripSplitter pose_split_;
  StageView stage_;
  // The timeline with the play controls above it.
  QWidget timeline_host_;
  QVBoxLayout timeline_layout_;
  TransportBar pose_transport_;
  TimelineView timeline_;
  GripSplitter rig_split_;
  RigCanvas rig_canvas_;
  // Scrolls the rig settings, so a piece with more of them never
  // squeezes or clips the rest. Before the panel, so the panel is gone
  // before the scroll area that holds it.
  QScrollArea rig_scroll_;
  RigPanel rig_panel_;
  CompPage comp_page_;
  VideoPage video_page_;
  QDockWidget cast_dock_;
  CastPanel cast_;
  QDockWidget inspector_dock_;
  Inspector inspector_;
  DockGrips dock_grips_;
  // Floats over the pages, so it comes after the panels it floats over.
  ToastView toast_view_;
  // A burst of red pixels over everything, when something big goes well.
  SuccessBurst burst_;
  // A greyed control clicked anyway shakes and says why.
  RefusalShaker shaker_;
  ExportDialog export_dialog_;
  TimingChartDialog timing_dialog_;
  QAction timing_action_;
  // Ctrl+Shift+C and V: copy and paste a part's settings, not its pose.
  SettingsActions settings_actions_;
  // Alt+N and Alt+Shift+N: a new storyboard frame, or draw over one.
  StoryboardMenu storyboard_menu_;
  // Ctrl+] and Ctrl+[: the pick's draw order one place forward or back.
  OrderMenu order_menu_;
  // The 1 to 4 keys that space the picked keys.
  SpaceKeysShortcuts space_keys_;
  QAction export_action_;
  QMenu play_menu_;
  QAction play_action_;
  QAction next_action_;
  QAction back_action_;
  QMenu help_menu_;
  QAction find_action_;
  QAction sheet_action_;
  CommandPalette palette_;
  ShortcutSheet sheet_;
  // Whether the posing panels were open before the Video tab hid them.
  bool was_cast_open_ = true;
  bool was_inspector_open_ = true;
};

}  // namespace snapper

#endif  // SNAPPER_UI_MAIN_WINDOW_H_
