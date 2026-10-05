#include "ui/main_window.h"

#include <QKeySequence>
#include <QMenuBar>

#include <cassert>

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

}  // namespace

MainWindow::MainWindow(HistoryManager* history)
    : history_(history),
      file_menu_(tr("&File")),
      edit_menu_(tr("&Edit")),
      quit_action_(tr("&Quit")),
      undo_action_(tr("&Undo")),
      redo_action_(tr("&Redo")) {
  assert(history_ != nullptr);
  BuildMenus();
  connect(history_, &HistoryManager::Changed, this, &MainWindow::Refresh);
  resize(1280, 800);
  Refresh();
  assert(windowTitle().contains(QStringLiteral("Snapper")));
}

void MainWindow::BuildMenus() {
  assert(history_ != nullptr);
  assert(menuBar() != nullptr);
  quit_action_.setShortcut(QKeySequence::Quit);
  undo_action_.setShortcut(QKeySequence::Undo);
  redo_action_.setShortcut(QKeySequence::Redo);
  file_menu_.addAction(&quit_action_);
  edit_menu_.addAction(&undo_action_);
  edit_menu_.addAction(&redo_action_);
  // Disabled actions still show why on hover.
  edit_menu_.setToolTipsVisible(true);
  menuBar()->addMenu(&file_menu_);
  menuBar()->addMenu(&edit_menu_);
  connect(&quit_action_, &QAction::triggered, this, &QWidget::close);
  connect(&undo_action_, &QAction::triggered, history_,
          &HistoryManager::Undo);
  connect(&redo_action_, &QAction::triggered, history_,
          &HistoryManager::Redo);
}

void MainWindow::Refresh() {
  assert(history_ != nullptr);
  const Project& project = history_->current();
  assert(!project.name.isEmpty());
  setWindowTitle(project.name + QStringLiteral("[*] - Snapper"));
  setWindowModified(history_->IsDirty());
  ShowStep(&undo_action_, tr("&Undo"), history_->UndoLabel(),
           history_->WhyNoUndo());
  ShowStep(&redo_action_, tr("&Redo"), history_->RedoLabel(),
           history_->WhyNoRedo());
}

}  // namespace snapper
