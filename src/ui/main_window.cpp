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
  rig_empty_.setAlignment(Qt::AlignCenter);
  rig_empty_.setEnabled(false);
  rig_empty_.setText(tr("The rig editor goes here."));
  pages_.addWidget(&rig_empty_);
  const auto show_problem = [this](const QString& why) {
    statusBar()->showMessage(why, kStatusMs);
  };
  connect(&stage_, &StageView::Problem, this, show_problem);
  connect(&timeline_, &TimelineView::Problem, this, show_problem);
  connect(&strip_, &ShotStrip::Problem, this, show_problem);
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
  const ShotMoment moment = Locate(project, managers_.playback->frame());
  const bool is_on_shot = moment.shot >= 0;
  if (is_on_shot) {
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
  redo_action_.setShortcut(QKeySequence::Redo);
  edit_menu_.addAction(&undo_action_);
  edit_menu_.addAction(&redo_action_);
  // Disabled actions still show why on hover.
  edit_menu_.setToolTipsVisible(true);
  play_action_.setShortcut(Qt::Key_Space);
  next_action_.setShortcuts({Qt::Key_Right, Qt::Key_Period});
  back_action_.setShortcuts({Qt::Key_Left, Qt::Key_Comma});
  play_menu_.addActions({&play_action_, &next_action_, &back_action_});
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
