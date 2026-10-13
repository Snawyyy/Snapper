#include "ui/main_window.h"

#include <QDir>
#include <QFileInfo>

#include <QCloseEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QStatusBar>

#include <cassert>

#include "edit/document_manager.h"
#include "anim/master_timeline.h"
#include "edit/export_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "ui/scroll_fit.h"
#include "ui/bar_text.h"
#include "ui/timing_chart_action.h"
#include "ui/toast_manager.h"

namespace snapper {
namespace {

// Puts a step's words, tooltip and enabled state on its menu entry.
void ShowStep(QAction* action, const StepFace& face) {
  assert(action != nullptr);
  assert(!face.text.isEmpty());
  action->setEnabled(face.enabled);
  action->setText(face.text);
  action->setToolTip(face.tooltip);
}

}  // namespace

MainWindow::MainWindow(const Managers& managers)
    : managers_(managers),
      file_menu_(managers.document, managers.history, this),
      krita_menu_(managers.library, managers.toasts, this),
      color_palette_(managers.palette, managers.document->SettingsPath(),
                     this),
      color_palette_action_(tr("Color &palette")),
      edit_menu_(tr("&Edit")),
      view_menu_(tr("&View")),
      motion_menu_(managers.motion),
      sound_(QFileInfo(managers.document->SettingsPath())
                 .dir()
                 .filePath(QStringLiteral("sounds")),
             managers.document->SettingsPath(), this),
      sounds_action_(tr("&Sounds")),
      undo_action_(tr("&Undo")),
      redo_action_(tr("&Redo")),
      top_bar_host_(tr("Top bar")),
      top_bar_(managers),
      pose_layout_(&pose_page_),
      strip_(managers),
      pose_split_(Qt::Vertical),
      stage_(managers),
      timeline_layout_(&timeline_host_),
      pose_transport_(managers),
      timeline_(managers),
      rig_split_(Qt::Horizontal),
      rig_canvas_(managers),
      rig_panel_(managers),
      comp_page_(managers),
      video_page_(managers),
      cast_dock_(tr("Cast")),
      cast_(managers),
      inspector_dock_(tr("Inspector")),
      inspector_(managers),
      toast_view_(managers.toasts, managers.motion, this, &pages_),
      burst_(managers.toasts, managers.motion, this),
      shaker_(managers.motion),
      export_dialog_(managers, this),
      timing_dialog_(managers, this),
      timing_action_(tr("Define &timing chart...")),
      settings_actions_(managers),
      space_keys_(this, managers),
      export_action_(tr("&Export...")),
      play_menu_(tr("&Play")),
      play_action_(tr("&Play / pause")),
      next_action_(tr("&Next frame")),
      back_action_(tr("Pre&vious frame")),
      help_menu_(tr("&Help")),
      find_action_(tr("&Find a command...")),
      sheet_action_(tr("&Keys and gestures...")),
      palette_(menuBar(), this),
      sheet_(menuBar(), this) {
  assert(managers_.IsComplete());
  setAttribute(Qt::WA_Hover);
  BuildMenus();
  BuildPages();
  connect(&rig_panel_, &RigPanel::DollPicked, &rig_canvas_,
          &RigCanvas::SetDoll);
  connect(&rig_panel_, &RigPanel::PickChanged, &rig_canvas_,
          &RigCanvas::SetPick);
  connect(&rig_panel_, &RigPanel::SkinPicked, &rig_canvas_,
          &RigCanvas::SetSkin);
  connect(&rig_canvas_, &RigCanvas::PickChanged, &rig_panel_,
          &RigPanel::SetPick);
  rig_canvas_.SetDoll(rig_panel_.doll());
  ToastManager* toasts = managers_.toasts;
  const auto show_problem = [toasts](const QString& why) {
    toasts->Problem(why);
  };
  connect(managers_.exporter, &ExportManager::Finished, this,
          [toasts](const QString& path) {
            toasts->Cheer(QObject::tr("Saved %1").arg(path));
          });
  connect(&stage_, &StageView::Problem, this, show_problem);
  connect(&timeline_, &TimelineView::Problem, this, show_problem);
  connect(&strip_, &ShotStrip::Problem, this, show_problem);
  connect(&cast_, &CastPanel::Problem, this, show_problem);
  connect(&rig_canvas_, &RigCanvas::Problem, this, show_problem);
  // A hint stays until the canvas clears it.
  connect(&rig_canvas_, &RigCanvas::Hint, this, [this](const QString& hint) {
    statusBar()->showMessage(hint);
  });
  connect(&rig_panel_, &RigPanel::Problem, this, show_problem);
  connect(&video_page_, &VideoPage::Problem, this, show_problem);
  connect(&comp_page_, &CompPage::Problem, this, show_problem);
  cast_dock_.setObjectName(QStringLiteral("cast"));
  cast_dock_.setWidget(&cast_);
  addDockWidget(Qt::LeftDockWidgetArea, &cast_dock_);
  connect(&inspector_, &Inspector::Problem, this, show_problem);
  inspector_dock_.setObjectName(QStringLiteral("inspector"));
  inspector_dock_.setWidget(&inspector_);
  addDockWidget(Qt::RightDockWidgetArea, &inspector_dock_);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this,
          &MainWindow::FollowPlayhead);
  BuildModes();
  connect(managers_.history, &HistoryManager::Changed, this,
          &MainWindow::Refresh);
  connect(managers_.document, &DocumentManager::PathChanged, this,
          &MainWindow::Refresh);
  connect(managers_.document, &DocumentManager::AutosaveFailed, this,
          [toasts](const QString& why) {
            toasts->Problem(tr("Autosave failed: %1").arg(why));
          });
  setDockNestingEnabled(true);
  resize(1440, 900);
  Refresh();
  assert(windowTitle().contains(QStringLiteral("Snapper")));
}

void MainWindow::Start(const QString& path) {
  assert(isVisible());
  assert(managers_.IsComplete());
  const bool has_path = !path.isEmpty();
  if (has_path) {
    file_menu_.OpenPath(path);
    return;
  }
  file_menu_.OfferRecovery();
}

void MainWindow::ShowMode(int index) {
  assert(index >= 0 && index <= static_cast<int>(Mode::kRig));
  assert(managers_.playback != nullptr);
  const int was = pages_.currentIndex();
  // Video and Comp both work on the reel; Pose and Rig on the shots.
  const bool is_on_reel = index == static_cast<int>(Mode::kVideo) ||
                          index == static_cast<int>(Mode::kComp);
  pages_.setCurrentIndex(index);
  managers_.playback->SetTimeline(is_on_reel ? Timeline::kReel
                                             : Timeline::kShots);
  // The cast shows on the Pose and Rig tabs, the inspector only on Pose;
  // each comes back as it was left there.
  const auto has_cast = [](int mode) {
    return mode == static_cast<int>(Mode::kPose) ||
           mode == static_cast<int>(Mode::kRig);
  };
  const auto has_inspector = [](int mode) {
    return mode == static_cast<int>(Mode::kPose);
  };
  if (has_cast(was) && !has_cast(index)) {
    was_cast_open_ = cast_dock_.isVisible();
    cast_dock_.hide();
  }
  if (!has_cast(was) && has_cast(index)) {
    cast_dock_.setVisible(was_cast_open_);
  }
  if (has_inspector(was) && !has_inspector(index)) {
    was_inspector_open_ = inspector_dock_.isVisible();
    inspector_dock_.hide();
  }
  if (!has_inspector(was) && has_inspector(index)) {
    inspector_dock_.setVisible(was_inspector_open_);
  }
  // Undo and redo act on a shot in the other tabs, on the newest change
  // here.
  Refresh();
}

void MainWindow::FollowPlayhead() {
  assert(managers_.playback != nullptr);
  assert(managers_.selection != nullptr);
  const bool is_on_shots = managers_.playback->timeline() == Timeline::kShots;
  if (!is_on_shots) {
    return;
  }
  const Project& project = managers_.history->current();
  const ShotMoment moment =
      Locate(project, managers_.playback->FrameOn(Timeline::kShots));
  const bool is_moving_on = moment.shot >= 0;
  if (is_moving_on) {
    managers_.selection->SelectShot(
        project.shots[static_cast<size_t>(moment.shot)]->id);
  }
}

bool MainWindow::event(QEvent* event) {
  assert(event != nullptr);
  assert(centralWidget() != nullptr || event->type() != QEvent::Show);
  const QEvent::Type type = event->type();
  const bool is_pointer_moving =
      type == QEvent::HoverMove || type == QEvent::HoverEnter ||
      type == QEvent::HoverLeave || type == QEvent::MouseMove ||
      type == QEvent::Leave;
  const bool result = QMainWindow::event(event);
  if (is_pointer_moving) {
    update();
  }
  return result;
}

void MainWindow::closeEvent(QCloseEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  const bool may_close = file_menu_.ConfirmDiscard();
  if (may_close) {
    event->accept();
  } else {
    event->ignore();
  }
}

void MainWindow::BuildPages() {
  assert(pages_.count() == 0);
  assert(managers_.IsComplete());
  pose_split_.addWidget(&stage_);
  timeline_layout_.setContentsMargins(0, 0, 0, 0);
  timeline_layout_.setSpacing(0);
  timeline_layout_.addWidget(&pose_transport_, 0, Qt::AlignLeft);
  timeline_layout_.addWidget(&timeline_, 1);
  pose_split_.addWidget(&timeline_host_);
  pose_split_.setStretchFactor(0, 3);
  pose_split_.setStretchFactor(1, 1);
  pose_layout_.setContentsMargins(0, 0, 0, 0);
  pose_layout_.setSpacing(0);
  pose_layout_.addWidget(&strip_);
  pose_layout_.addWidget(&pose_split_, 1);
  rig_split_.addWidget(&rig_canvas_);
  rig_scroll_.setWidgetResizable(true);
  rig_scroll_.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  rig_scroll_.setFrameShape(QFrame::NoFrame);
  rig_scroll_.setWidget(&rig_panel_);
  // As wide as the settings need, whatever piece they show.
  KeepWideEnough(&rig_scroll_);
  rig_split_.addWidget(&rig_scroll_);
  rig_split_.setCollapsible(1, false);
  rig_split_.setStretchFactor(0, 3);
  rig_split_.setStretchFactor(1, 1);
  pages_.addWidget(&video_page_);
  pages_.addWidget(&pose_page_);
  pages_.addWidget(&comp_page_);
  pages_.addWidget(&rig_split_);
  setCentralWidget(&pages_);
  assert(pages_.count() == static_cast<int>(Mode::kRig) + 1);
}

void MainWindow::BuildModes() {
  assert(top_bar_.tabs()->count() == pages_.count());
  assert(pages_.count() == static_cast<int>(Mode::kRig) + 1);
  top_bar_host_.setObjectName(QStringLiteral("modes_bar"));
  top_bar_host_.setMovable(false);
  top_bar_host_.setFloatable(false);
  // The tabs are the only way between modes, so they cannot be hidden.
  top_bar_host_.toggleViewAction()->setVisible(false);
  top_bar_host_.addWidget(&top_bar_);
  addToolBar(Qt::TopToolBarArea, &top_bar_host_);
  // The app opens on Pose.
  top_bar_.tabs()->setCurrentIndex(static_cast<int>(Mode::kPose));
  pages_.setCurrentIndex(static_cast<int>(Mode::kPose));
  connect(top_bar_.tabs(), &QTabBar::currentChanged, this,
          &MainWindow::ShowMode);
  assert(top_bar_.tabs()->currentIndex() == static_cast<int>(Mode::kPose));
  assert(pages_.currentIndex() == static_cast<int>(Mode::kPose));
}

void MainWindow::BuildMenus() {
  assert(managers_.history != nullptr);
  assert(menuBar() != nullptr);
  undo_action_.setShortcut(QKeySequence::Undo);
  // Ctrl+Y everywhere, plus the system's own (Ctrl+Shift+Z on Linux).
  redo_action_.setShortcuts(
      {QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(QKeySequence::Redo)});
  edit_menu_.addAction(&undo_action_);
  edit_menu_.addAction(&redo_action_);
  edit_menu_.addSeparator();
  timing_action_.setShortcut(TimingChartShortcut());
  edit_menu_.addAction(&timing_action_);
  connect(&timing_action_, &QAction::triggered, this,
          [this] { RequestTimingChart(managers_); });
  edit_menu_.addActions(settings_actions_.actions());
  // Disabled actions still show why on hover.
  edit_menu_.setToolTipsVisible(true);
  view_menu_.setToolTipsVisible(true);
  play_menu_.setToolTipsVisible(true);
  help_menu_.setToolTipsVisible(true);
  play_action_.setShortcut(Qt::Key_Space);
  next_action_.setShortcuts({Qt::Key_Right, Qt::Key_Period});
  back_action_.setShortcuts({Qt::Key_Left, Qt::Key_Comma});
  play_menu_.addActions({&play_action_, &next_action_, &back_action_});
  export_action_.setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
  QMenu* file = file_menu_.menu();
  // Export sits just above Quit.
  file->insertAction(file->actions().last(), &export_action_);
  file->insertMenu(file->actions().last(), krita_menu_.menu());
  file->insertAction(file->actions().last(), krita_menu_.program_action());
  file->insertSeparator(file->actions().last());
  connect(&cast_, &CastPanel::OpenInKritaRequested, &krita_menu_,
          &KritaMenu::Open);
  connect(&export_action_, &QAction::triggered, &export_dialog_, [this] {
    export_dialog_.show();
    export_dialog_.raise();
  });
  menuBar()->addMenu(file_menu_.menu());
  menuBar()->addMenu(&edit_menu_);
  view_menu_.addMenu(motion_menu_.menu());
  menuBar()->addMenu(&view_menu_);
  menuBar()->addMenu(&play_menu_);
  BuildViewAndHelp();
  PlaybackManager* playback = managers_.playback;
  connect(&play_action_, &QAction::triggered, playback,
          &PlaybackManager::Toggle);
  connect(&next_action_, &QAction::triggered, playback,
          [playback] { playback->Step(1); });
  connect(&back_action_, &QAction::triggered, playback,
          [playback] { playback->Step(-1); });
  connect(&undo_action_, &QAction::triggered, this, &MainWindow::UndoStep);
  connect(&redo_action_, &QAction::triggered, this, &MainWindow::RedoStep);
  connect(&top_bar_, &TopBar::UndoRequested, this, &MainWindow::UndoStep);
  connect(&top_bar_, &TopBar::RedoRequested, this, &MainWindow::RedoStep);
  connect(managers_.selection, &SelectionManager::Changed, this,
          &MainWindow::Refresh);
  for (const QAction* step : {&undo_action_, &redo_action_}) {
    connect(step, &QAction::triggered, &sound_,
            [this] { sound_.Play(UiSound::Blip::kTick); });
  }
}

void MainWindow::BuildViewAndHelp() {
  assert(menuBar() != nullptr);
  assert(managers_.toasts != nullptr);
  sounds_action_.setCheckable(true);
  sounds_action_.setChecked(sound_.is_on());
  sounds_action_.setToolTip(tr("Quiet ticks and chimes for undo, redo and "
                               "big wins."));
  view_menu_.addAction(&sounds_action_);
  color_palette_action_.setCheckable(true);
  color_palette_action_.setToolTip(
      tr("The floating palette: the picked piece's and doll's colors, "
         "recent and added ones."));
  view_menu_.addAction(&color_palette_action_);
  connect(&color_palette_action_, &QAction::toggled, &color_palette_,
          &PalettePanel::SetOpen);
  connect(&color_palette_, &PalettePanel::OpenChanged,
          &color_palette_action_, &QAction::setChecked);
  connect(&sounds_action_, &QAction::toggled, &sound_, &UiSound::SetOn);
  connect(managers_.toasts, &ToastManager::Cheered, &sound_,
          [this] { sound_.Play(UiSound::Blip::kChime); });
  find_action_.setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash));
  sheet_action_.setShortcuts({QKeySequence(Qt::Key_F1),
                              QKeySequence(Qt::CTRL | Qt::SHIFT |
                                           Qt::Key_Question)});
  help_menu_.addActions({&find_action_, &sheet_action_});
  menuBar()->addMenu(&help_menu_);
  connect(&find_action_, &QAction::triggered, &palette_,
          &CommandPalette::Open);
  connect(&sheet_action_, &QAction::triggered, &sheet_,
          &ShortcutSheet::Open);
}

void MainWindow::Refresh() {
  assert(managers_.history != nullptr);
  const Project& project = managers_.history->current();
  assert(!project.name.isEmpty());
  const QString& path = managers_.document->path();
  const QString where = path.isEmpty() ? tr("not saved yet") : path;
  setWindowTitle(QStringLiteral("%1[*] (%2) - Snapper")
                     .arg(project.name, where));
  setWindowModified(managers_.history->IsDirty());
  const HistoryManager& history = *managers_.history;
  // Undo and redo name and act on the shot being worked in.
  const ShotId shot = UndoShot();
  const bool is_scoped = shot.IsValid();
  const QString undo_label =
      is_scoped ? history.UndoLabelIn(shot) : history.UndoLabel();
  const QString undo_why =
      is_scoped ? history.WhyNoUndoIn(shot) : history.WhyNoUndo();
  const QString redo_label =
      is_scoped ? history.RedoLabelIn(shot) : history.RedoLabel();
  const QString redo_why =
      is_scoped ? history.WhyNoRedoIn(shot) : history.WhyNoRedo();
  ShowStep(&undo_action_, MakeStepFace(tr("&Undo"), undo_label, undo_why));
  ShowStep(&redo_action_, MakeStepFace(tr("&Redo"), redo_label, redo_why));
  top_bar_.ShowUndo(MakeStepFace(tr("Undo"), undo_label, undo_why));
  top_bar_.ShowRedo(MakeStepFace(tr("Redo"), redo_label, redo_why));
}

ShotId MainWindow::UndoShot() const {
  assert(managers_.selection != nullptr);
  assert(pages_.count() > 0);
  const bool is_video =
      pages_.currentIndex() == static_cast<int>(Mode::kVideo);
  return is_video ? ShotId() : managers_.selection->shot();
}

void MainWindow::UndoStep() {
  assert(managers_.history != nullptr);
  assert(managers_.selection != nullptr);
  const ShotId shot = UndoShot();
  const bool is_scoped = shot.IsValid();
  if (is_scoped) {
    managers_.history->UndoIn(shot);
  } else {
    managers_.history->Undo();
  }
}

void MainWindow::RedoStep() {
  assert(managers_.history != nullptr);
  assert(managers_.selection != nullptr);
  const ShotId shot = UndoShot();
  const bool is_scoped = shot.IsValid();
  if (is_scoped) {
    managers_.history->RedoIn(shot);
  } else {
    managers_.history->Redo();
  }
}

}  // namespace snapper
