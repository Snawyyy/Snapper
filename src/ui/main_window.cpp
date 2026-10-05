#include "ui/main_window.h"

#include <QCloseEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QStatusBar>

#include <cassert>

#include "edit/document_manager.h"
#include "edit/history_manager.h"

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
      center_layout_(&center_) {
  assert(managers_.IsComplete());
  BuildMenus();
  modes_.addTab(tr("Pose"));
  modes_.addTab(tr("Rig"));
  modes_.setExpanding(false);
  for (QLabel* empty : {&pose_empty_, &rig_empty_}) {
    empty->setAlignment(Qt::AlignCenter);
    empty->setEnabled(false);
    pages_.addWidget(empty);
  }
  pose_empty_.setText(tr("The stage goes here."));
  rig_empty_.setText(tr("The rig editor goes here."));
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
  menuBar()->addMenu(file_menu_.menu());
  menuBar()->addMenu(&edit_menu_);
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
