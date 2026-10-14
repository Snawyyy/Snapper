// CompTimeline: keys, the right click menu, and what they do to the
// picked clips, each one undo step.

#include <QAction>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>

#include <cassert>
#include <map>
#include <set>
#include <vector>

#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
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
  std::map<CompClipRef, Span> spans;
  for (const Bar& bar : Bars()) {
    const bool is_picked = picks.contains(bar.ref);
    if (is_picked) {
      spans[bar.ref] = Span{bar.first, bar.end, bar.offset};
    }
  }
  const int kept = KeptOnReel(spans, delta);
  const bool is_stuck = kept == 0;
  if (is_stuck) {
    Report(tr("The picked clips are at the edge of the reel."));
    return;
  }
  EditScope scope(managers_.history, tr("Nudge effects"));
  for (const auto& [ref, span] : spans) {
    Report(ProblemOf(managers_.comp->SetTiming(
        ref.shot, ref.id, span.first + kept, span.end - span.first, true)));
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
  const bool is_none = picks.empty();
  if (is_none) {
    Report(tr("Pick an effect first."));
    return;
  }
  EditScope scope(managers_.history,
                  picks.size() == 1 ? tr("Remove effect")
                                    : tr("Remove %1 effects")
                                          .arg(picks.size()));
  for (const CompClipRef& ref : picks) {
    Report(ProblemOf(managers_.comp->Remove(ref.shot, ref.id)));
  }
}

Frame CompTimeline::Playhead() const {
  assert(managers_.playback != nullptr);
  assert(managers_.comp != nullptr);
  return managers_.playback->FrameOn(Timeline::kReel);
}

void CompTimeline::SplitPicked() {
  assert(managers_.comp != nullptr);
  assert(managers_.playback != nullptr);
  Report(ProblemOf(
      managers_.comp->SplitAll(managers_.comp->ClipPicks(), Playhead())));
}

bool CompTimeline::CopyPicked() {
  assert(managers_.comp != nullptr);
  assert(managers_.history != nullptr);
  const Result<void> copied =
      managers_.comp->CopyClips(managers_.comp->ClipPicks());
  Report(ProblemOf(copied));
  return copied.has_value();
}

void CompTimeline::CutPicked() {
  assert(managers_.comp != nullptr);
  assert(managers_.history != nullptr);
  Report(ProblemOf(managers_.comp->CutAll(managers_.comp->ClipPicks())));
}

void CompTimeline::PasteAtPlayhead() {
  assert(managers_.comp != nullptr);
  assert(managers_.selection != nullptr);
  // A copy pasted into another shot works on the stage pick there, or
  // on the screen when that pick draws nothing.
  const auto target = managers_.comp->TargetFor(
      managers_.selection->shot(), managers_.selection->layer(),
      managers_.selection->pieces());
  Report(ProblemOf(managers_.comp->PasteAt(
      Playhead(), target.value_or(CompTarget{}))));
}

void CompTimeline::DuplicatePicked() {
  assert(managers_.comp != nullptr);
  assert(managers_.history != nullptr);
  Report(
      ProblemOf(managers_.comp->DuplicateAll(managers_.comp->ClipPicks())));
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
  const bool is_control = mods.testFlag(Qt::ControlModifier);
  const bool is_zoom_in = is_control && (key == Qt::Key_Equal ||
                                         key == Qt::Key_Plus);
  const bool is_zoom_out = is_control && key == Qt::Key_Minus;
  const bool is_zoom = is_zoom_in || is_zoom_out;
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
  } else if (is_zoom) {
    const double playhead = XOf(managers_.playback->FrameOn(
        Timeline::kReel).index());
    ZoomBy(is_zoom_in ? 1.5 : 1.0 / 1.5, playhead);
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
  QMenu menu;
  FillMenu(&menu);
  const QAction* chosen = menu.exec(event->globalPos());
  const bool is_chosen = chosen != nullptr;
  if (is_chosen) {
    RunMenu(static_cast<MenuCommand>(chosen->data().toInt()));
  }
}

void CompTimeline::FillMenu(QMenu* menu) const {
  assert(menu != nullptr);
  assert(managers_.comp != nullptr);
  const CompManager* comp = managers_.comp;
  const std::set<CompClipRef> picks = comp->ClipPicks();
  const size_t count = picks.size();
  const QString why_none =
      count == 0 ? tr("Pick an effect clip first.") : QString();
  const QString copied = comp->CopiedName();
  struct Entry final {
    MenuCommand command;
    QString name;
    QString why_not;
  };
  const std::vector<Entry> entries = {
      {MenuCommand::kSplit, tr("Split at playhead\tS"),
       comp->WhyNoSplit(picks, Playhead())},
      {MenuCommand::kCopy, tr("Copy\tCtrl+C"), why_none},
      {MenuCommand::kCut, tr("Cut\tCtrl+X"), why_none},
      {MenuCommand::kPaste,
       copied.isEmpty() ? tr("Paste at playhead\tCtrl+V")
                        : tr("Paste %1 at playhead\tCtrl+V").arg(copied),
       comp->WhyNoPasteAt(Playhead())},
      {MenuCommand::kDuplicate, tr("Duplicate\tCtrl+D"), why_none},
      {MenuCommand::kNone, QString(), QString()},
      {MenuCommand::kFit, tr("Fit to shot"), why_none},
      {MenuCommand::kNudgeBack, tr("Nudge back\tAlt+Left"), why_none},
      {MenuCommand::kNudgeOn, tr("Nudge on\tAlt+Right"), why_none},
      {MenuCommand::kNone, QString(), QString()},
      {MenuCommand::kRemove,
       count > 1 ? tr("Remove %1 effects\tDelete").arg(count)
                 : tr("Remove\tDelete"),
       why_none},
      {MenuCommand::kNone, QString(), QString()},
      {MenuCommand::kAll, tr("Pick all\tCtrl+A"), QString()},
      {MenuCommand::kWholeReel, tr("Show the whole reel"), QString()},
  };
  for (const Entry& entry : entries) {
    const bool is_line = entry.command == MenuCommand::kNone;
    if (is_line) {
      menu->addSeparator();
      continue;
    }
    QAction* action = menu->addAction(entry.name);
    action->setData(static_cast<int>(entry.command));
    Explain(action, entry.why_not);
  }
}

void CompTimeline::RunMenu(MenuCommand command) {
  assert(command != MenuCommand::kNone);
  assert(managers_.comp != nullptr);
  switch (command) {
    case MenuCommand::kSplit: SplitPicked(); break;
    case MenuCommand::kCopy: CopyPicked(); break;
    case MenuCommand::kCut: CutPicked(); break;
    case MenuCommand::kPaste: PasteAtPlayhead(); break;
    case MenuCommand::kDuplicate: DuplicatePicked(); break;
    case MenuCommand::kFit: FitPicked(); break;
    case MenuCommand::kNudgeBack: Nudge(-1); break;
    case MenuCommand::kNudgeOn: Nudge(1); break;
    case MenuCommand::kRemove: RemovePicked(); break;
    case MenuCommand::kAll:
      managers_.comp->PickClips(managers_.comp->AllClips(),
                                PickMode::kReplace, CompClipRef());
      break;
    case MenuCommand::kWholeReel:
      shown_ = 0.0;
      first_ = 0.0;
      update();
      break;
    case MenuCommand::kNone:
      break;
  }
}

}  // namespace snapper
