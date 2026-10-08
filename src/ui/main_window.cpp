#include "ui/main_window.h"

#include <QCloseEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QStatusBar>

#include <cassert>

#include "edit/document_manager.h"
#include "anim/master_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"

namespace snapper {
namespace {

// "Undo Rotate head" when it can act, else greyed out saying why.
void ShowStep(QAction* action, const QString& verb, const QString& label,
              const QString& why_not) {
  assert(action != nullptr);
  assert(!verb.isEmpty());
  const bool can_act = why_not.isEmpty();
  action->setEnabled(can_act);
  action->setText(can_act ? verb + QLatin1Char(' ') + label : verb);
  action->setToolTip(why_not);
}

constexpr int kStatusMs = 6000;

}  // namespace

MainWindow::MainWindow(const Managers& managers)
    : managers_(managers),
      file_menu_(managers.document, managers.history, this),
      edit_menu_(tr("&Edit")),
      undo_action_(tr("&Undo")),
      redo_action_(tr("&Redo")),
      center_layout_(&center_),
      pose_layout_(&pose_page_),
      strip_(managers),
      pose_split_(Qt::Vertical),
      stage_(managers),
      timeline_(managers),
      rig_split_(Qt::Horizontal),
      rig_canvas_(managers),
      rig_panel_(managers),
      cast_dock_(tr("Cast")),
      cast_(managers),
      inspector_dock_(tr("Inspector")),
      inspector_(managers),
      export_dialog_(managers, this),
      export_action_(tr("&Export...")),
      play_menu_(tr("&Play")),
      play_action_(tr("&Play / pause")),
      next_action_(tr("&Next frame")),
      back_action_(tr("Pre&vious frame")) {
  assert(managers_.IsComplete());
  BuildMenus();
  modes_.addTab(tr("Pose"));
  modes_.addTab(tr("Rig"));
  modes_.setExpanding(false);
  pose_split_.addWidget(&stage_);
  pose_split_.addWidget(&timeline_);
  pose_split_.setStretchFactor(0, 3);
  pose_split_.setStretchFactor(1, 1);
  pose_layout_.setContentsMargins(0, 0, 0, 0);
  pose_layout_.setSpacing(0);
  pose_layout_.addWidget(&strip_);
  pose_layout_.addWidget(&pose_split_, 1);
  pages_.addWidget(&pose_page_);
  rig_split_.addWidget(&rig_canvas_);
  rig_split_.addWidget(&rig_panel_);
  rig_split_.setStretchFactor(0, 3);
  rig_split_.setStretchFactor(1, 1);
  pages_.addWidget(&rig_split_);
  connect(&rig_panel_, &RigPanel::DollPicked, &rig_canvas_,
          &RigCanvas::SetDoll);
  connect(&rig_panel_, &RigPanel::PickChanged, &rig_canvas_,
          &RigCanvas::SetPick);
  connect(&rig_canvas_, &RigCanvas::PickChanged, &rig_panel_,
          &RigPanel::SetPick);
  rig_canvas_.SetDoll(rig_panel_.doll());
  const auto show_problem = [this](const QString& why) {
    statusBar()->showMessage(why, kStatusMs);
  };
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
  cast_dock_.setObjectName(QStringLiteral("cast"));
  cast_dock_.setWidget(&cast_);
  addDockWidget(Qt::LeftDockWidgetArea, &cast_dock_);
  connect(&inspector_, &Inspector::Problem, this, show_problem);
  inspector_dock_.setObjectName(QStringLiteral("inspector"));
  inspector_dock_.setWidget(&inspector_);
  addDockWidget(Qt::RightDockWidgetArea, &inspector_dock_);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this,
          &MainWindow::FollowPlayhead);
  center_layout_.setContentsMargins(0, 0, 0, 0);
  center_layout_.setSpacing(0);
  center_layout_.addWidget(&modes_);
  center_layout_.addWidget(&pages_, 1);
  setCentralWidget(&center_);
  connect(&modes_, &QTabBar::currentChanged, &pages_,
          &QStackedWidget::setCurrentIndex);
  connect(managers_.history, &HistoryManager::Changed, this,
          &MainWindow::Refresh);
  connect(managers_.document, &DocumentManager::PathChanged, this,
          &MainWindow::Refresh);
  connect(managers_.document, &DocumentManager::AutosaveFailed, this,
          [this](const QString& why) {
            statusBar()->showMessage(tr("Autosave failed: %1").arg(why),
                                     kStatusMs);
          });
  setDockNestingEnabled(true);
  resize(1440, 900);
  Refresh();
  assert(windowTitle().contains(QStringLiteral("Snapper")));
}

void MainWindow::Start() {
  assert(isVisible());
  assert(managers_.IsComplete());
  file_menu_.OfferRecovery();
}

void MainWindow::FollowPlayhead() {
  assert(managers_.playback != nullptr);
  assert(managers_.selection != nullptr);
  const Project& project = managers_.history->current();
  const ShotMoment moment =
      Locate(project, managers_.playback->FrameOn(Timeline::kShots));
  const bool is_on_shot = moment.shot >= 0;
  // In a hand-over the picked shot may be either side; keep it.
  const bool is_next_picked =
      moment.next_shot >= 0 &&
      project.shots[static_cast<size_t>(moment.next_shot)]->id ==
          managers_.selection->shot();
  const bool is_moving_on = is_on_shot && !is_next_picked;
  if (is_moving_on) {
    managers_.selection->SelectShot(
        project.shots[static_cast<size_t>(moment.shot)]->id);
  }
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

void MainWindow::BuildMenus() {
  assert(managers_.history != nullptr);
  assert(menuBar() != nullptr);
  undo_action_.setShortcut(QKeySequence::Undo);
  // Ctrl+Y everywhere, plus the system's own (Ctrl+Shift+Z on Linux).
  redo_action_.setShortcuts(
      {QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(QKeySequence::Redo)});
  edit_menu_.addAction(&undo_action_);
  edit_menu_.addAction(&redo_action_);
  // Disabled actions still show why on hover.
  edit_menu_.setToolTipsVisible(true);
  play_action_.setShortcut(Qt::Key_Space);
  next_action_.setShortcuts({Qt::Key_Right, Qt::Key_Period});
  back_action_.setShortcuts({Qt::Key_Left, Qt::Key_Comma});
  play_menu_.addActions({&play_action_, &next_action_, &back_action_});
  export_action_.setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
  QMenu* file = file_menu_.menu();
  // Export sits just above Quit.
  file->insertAction(file->actions().last(), &export_action_);
  file->insertSeparator(file->actions().last());
  connect(&export_action_, &QAction::triggered, &export_dialog_, [this] {
    export_dialog_.show();
    export_dialog_.raise();
  });
  menuBar()->addMenu(file_menu_.menu());
  menuBar()->addMenu(&edit_menu_);
  menuBar()->addMenu(&play_menu_);
  PlaybackManager* playback = managers_.playback;
  connect(&play_action_, &QAction::triggered, playback,
          &PlaybackManager::Toggle);
  connect(&next_action_, &QAction::triggered, playback,
          [playback] { playback->Step(1); });
  connect(&back_action_, &QAction::triggered, playback,
          [playback] { playback->Step(-1); });
  connect(&undo_action_, &QAction::triggered, managers_.history,
          &HistoryManager::Undo);
  connect(&redo_action_, &QAction::triggered, managers_.history,
          &HistoryManager::Redo);
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
  ShowStep(&undo_action_, tr("&Undo"), managers_.history->UndoLabel(),
           managers_.history->WhyNoUndo());
  ShowStep(&redo_action_, tr("&Redo"), managers_.history->RedoLabel(),
           managers_.history->WhyNoRedo());
}

}  // namespace snapper
