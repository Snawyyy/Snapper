#include "ui/order_menu.h"

#include <QKeySequence>

#include <cassert>
#include <set>
#include <vector>

#include "edit/draw_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/pose_manager.h"
#include "edit/selection_manager.h"
#include "ui/playhead.h"
#include "ui/problem.h"
#include "ui/toast_manager.h"

namespace snapper {

OrderMenu::OrderMenu(const Managers& managers, QWidget* stage)
    : managers_(managers),
      stage_(stage),
      forward_(tr("Draw one place forward")),
      back_(tr("Draw one place back")) {
  assert(managers_.IsComplete());
  assert(stage_ != nullptr);
  forward_.setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
  back_.setShortcut(QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
  connect(&forward_, &QAction::triggered, this, [this] { Step(1); });
  connect(&back_, &QAction::triggered, this, [this] { Step(-1); });
  const auto refresh = [this] { Refresh(); };
  connect(managers_.history, &HistoryManager::Changed, this, refresh);
  connect(managers_.selection, &SelectionManager::Changed, this, refresh);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, refresh);
  const bool has_draw = managers_.draw != nullptr;
  if (has_draw) {
    connect(managers_.draw, &DrawManager::Started, this, refresh);
    connect(managers_.draw, &DrawManager::Ended, this, refresh);
  }
  Refresh();
}

QString OrderMenu::WhyNot() const {
  assert(managers_.history != nullptr);
  assert(stage_ != nullptr);
  const bool is_on_pose = stage_->isVisibleTo(stage_->window());
  if (!is_on_pose) {
    return tr("Open the Pose tab to change the draw order.");
  }
  const bool is_drawing = managers_.draw != nullptr &&
                          managers_.draw->is_drawing();
  if (is_drawing) {
    return tr("Finish drawing first.");
  }
  const auto spot = SpotOf(managers_);
  if (!spot) {
    return tr("Put the playhead in a shot first.");
  }
  const bool has_pick = PickedTrack(managers_, spot->shot).has_value();
  return has_pick ? QString()
                  : tr("Pick a piece or a doll on the stage first.");
}

void OrderMenu::Refresh() {
  assert(stage_ != nullptr);
  assert(managers_.history != nullptr);
  const QString why_not = WhyNot();
  const bool can_act = why_not.isEmpty();
  for (const bool is_forward : {true, false}) {
    QAction* action = is_forward ? &forward_ : &back_;
    action->setEnabled(can_act);
    action->setToolTip(
        can_act ? (is_forward ? tr("Draws the pick one place in front, "
                                   "keyed at the playhead.")
                              : tr("Draws the pick one place behind, keyed "
                                   "at the playhead."))
                : why_not);
  }
}

void OrderMenu::Step(int by) {
  assert(by == 1 || by == -1);
  assert(managers_.pose != nullptr);
  const QString why_not = WhyNot();
  const bool is_refused = !why_not.isEmpty();
  if (is_refused) {
    managers_.toasts->Problem(why_not);
    return;
  }
  const auto spot = SpotOf(managers_);
  // With keys picked, every picked frame steps, as in the Pose box.
  const std::set<Frame> picked = managers_.selection->EditFrames(spot->local);
  const std::vector<Frame> frames(picked.begin(), picked.end());
  const QString problem = ProblemOf(managers_.pose->StepOrder(
      managers_.selection->PickedTracks(), frames, by));
  const bool has_problem = !problem.isEmpty();
  if (has_problem) {
    managers_.toasts->Problem(problem);
  }
}

}  // namespace snapper
