#include "ui/storyboard_menu.h"

#include <QKeySequence>

#include <cassert>

#include "edit/draw_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "edit/storyboard_manager.h"
#include "ui/storyboard_actions.h"

namespace snapper {

StoryboardMenu::StoryboardMenu(const Managers& managers, QWidget* stage)
    : managers_(managers),
      stage_(stage),
      new_(tr("New storyboard frame here")),
      over_(tr("Draw over storyboard frame")) {
  assert(managers_.IsComplete());
  assert(stage_ != nullptr);
  new_.setShortcut(QKeySequence(Qt::ALT | Qt::Key_N));
  over_.setShortcut(QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_N));
  connect(&new_, &QAction::triggered, this,
          [this] { RequestStoryboard(managers_, false); });
  connect(&over_, &QAction::triggered, this,
          [this] { RequestStoryboard(managers_, true); });
  const bool is_available =
      managers_.storyboard != nullptr && managers_.draw != nullptr;
  if (is_available) {
    const auto refresh = [this] { Refresh(); };
    connect(managers_.history, &HistoryManager::Changed, this, refresh);
    connect(managers_.selection, &SelectionManager::Changed, this, refresh);
    connect(managers_.playback, &PlaybackManager::FrameChanged, this,
            refresh);
    connect(managers_.storyboard, &StoryboardManager::Changed, this,
            refresh);
    connect(managers_.draw, &DrawManager::Started, this, refresh);
    connect(managers_.draw, &DrawManager::Ended, this, refresh);
  }
  Refresh();
}

void StoryboardMenu::Refresh() {
  assert(stage_ != nullptr);
  assert(managers_.history != nullptr);
  const bool is_on_pose = stage_->isVisibleTo(stage_->window());
  for (const bool is_over : {false, true}) {
    QAction* action = is_over ? &over_ : &new_;
    const QString why_not =
        is_on_pose ? WhyNoStoryboardHere(managers_, is_over)
                   : tr("Open the Pose tab to draw the storyboard.");
    const bool can_act = why_not.isEmpty();
    action->setEnabled(can_act);
    const QString help =
        is_over ? tr("Draw on the storyboard frame showing at the "
                     "playhead.")
                : tr("A blank frame from the playhead on, with the frame "
                     "before faint to trace from.");
    action->setToolTip(can_act ? help : why_not);
  }
}

}  // namespace snapper
