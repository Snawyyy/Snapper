// CompTimeline: keys, the right click menu, and what they do to the
// picked clips, each one undo step.

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>

#include <cassert>
#include <map>

#include "edit/history_manager.h"
#include "ui/comp_timeline.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"

namespace snapper {

void CompTimeline::Nudge(int delta) {
  assert(delta != 0);
  assert(managers_.comp != nullptr);
  const std::set<CompClipRef> picks = managers_.comp->ClipPicks();
  const bool has_picks = !picks.empty();
  if (!has_picks) {
    Report(tr("Pick an effect clip to nudge it."));
    return;
  }
  EditScope scope(managers_.history, tr("Nudge effects"));
  for (const Bar& bar : Bars()) {
    const bool is_picked = picks.contains(bar.ref);
    if (is_picked) {
      Report(ProblemOf(managers_.comp->SetTiming(
          bar.ref.shot, bar.ref.id, bar.first + delta, bar.end - bar.first,
          true)));
    }
  }
}

void CompTimeline::FitPicked() {
  assert(managers_.comp != nullptr);
  assert(managers_.history != nullptr);
  EditScope scope(managers_.history, tr("Fit effects to shot"));
  for (const CompClipRef& ref : managers_.comp->ClipPicks()) {
    Report(ProblemOf(
        managers_.comp->SetTiming(ref.shot, ref.id, 0, kWholeShot, false)));
  }
}

void CompTimeline::RemovePicked() {
  assert(managers_.comp != nullptr);
  assert(managers_.history != nullptr);
  const std::set<CompClipRef> picks = managers_.comp->ClipPicks();
  EditScope scope(managers_.history,
                  picks.size() == 1 ? tr("Remove effect")
                                    : tr("Remove %1 effects")
                                          .arg(picks.size()));
  for (const CompClipRef& ref : picks) {
    Report(ProblemOf(managers_.comp->Remove(ref.shot, ref.id)));
  }
}

void CompTimeline::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  const int key = event->key();
  const Qt::KeyboardModifiers mods = event->modifiers();
  const bool is_escape = key == Qt::Key_Escape;
  const bool is_all =
      key == Qt::Key_A && mods.testFlag(Qt::ControlModifier);
  const bool is_nudge = mods.testFlag(Qt::AltModifier) &&
                        (key == Qt::Key_Left || key == Qt::Key_Right);
  const bool is_dragging = drag_ != nullptr;
  const bool is_cancel = is_escape && is_dragging;
  if (is_cancel) {
    drag_->Cancel();
    EndDrag();
  } else if (is_escape) {
    grab_ = Grab::kNone;
    managers_.comp->PickClips({}, PickMode::kReplace, CompClipRef());
  } else if (is_all) {
    managers_.comp->PickClips(managers_.comp->AllClips(),
                              PickMode::kReplace, CompClipRef());
  } else if (is_nudge) {
    const int step = mods.testFlag(Qt::ShiftModifier) ? 10 : 1;
    Nudge(key == Qt::Key_Left ? -step : step);
  } else {
    QWidget::keyPressEvent(event);
    return;
  }
  update();
}

void CompTimeline::contextMenuEvent(QContextMenuEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  const std::optional<Bar> bar = BarAt(event->pos());
  const bool is_on = bar.has_value();
  const bool is_unpicked =
      is_on && !managers_.comp->ClipPicks().contains(bar->ref);
  if (is_unpicked) {
    managers_.comp->PickClips({bar->ref}, PickMode::kReplace, bar->ref);
  }
  const size_t count = managers_.comp->ClipPicks().size();
  const QString why_none =
      count == 0 ? tr("Pick an effect clip first.") : QString();
  QMenu menu;
  QAction* fit = menu.addAction(tr("Fit to shot"));
  QAction* nudge_back = menu.addAction(tr("Nudge back\tAlt+Left"));
  QAction* nudge_on = menu.addAction(tr("Nudge on\tAlt+Right"));
  menu.addSeparator();
  QAction* remove = menu.addAction(
      count > 1 ? tr("Remove %1 effects\tDelete").arg(count)
                : tr("Remove\tDelete"));
  menu.addSeparator();
  QAction* all = menu.addAction(tr("Pick all\tCtrl+A"));
  QAction* fit_view = menu.addAction(tr("Show the whole reel"));
  for (QAction* action : {fit, nudge_back, nudge_on, remove}) {
    Explain(action, why_none);
  }
  const QAction* chosen = menu.exec(event->globalPos());
  const bool is_fit = chosen == fit;
  const bool is_back = chosen == nudge_back;
  const bool is_on_nudge = chosen == nudge_on;
  const bool is_remove = chosen == remove;
  const bool is_all = chosen == all;
  const bool is_fit_view = chosen == fit_view;
  const bool is_nudge = is_back || is_on_nudge;
  if (is_fit) {
    FitPicked();
  } else if (is_nudge) {
    Nudge(is_back ? -1 : 1);
  } else if (is_remove) {
    RemovePicked();
  } else if (is_all) {
    managers_.comp->PickClips(managers_.comp->AllClips(),
                              PickMode::kReplace, CompClipRef());
  } else if (is_fit_view) {
    shown_ = 0.0;
    first_ = 0.0;
    update();
  }
}

}  // namespace snapper
