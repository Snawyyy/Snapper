// ReelTimeline's keys and menus: the edits on picked clips.

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>

#include <array>
#include <cassert>
#include <set>
#include <vector>

#include "anim/reel_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "ui/form_helpers.h"
#include "ui/reel_timeline.h"

namespace snapper {

ReelTimeline::Command ReelTimeline::CommandFor(const QKeyEvent& event) {
  assert(event.type() == QEvent::KeyPress);
  const int key = event.key();
  assert(key != 0);
  const Qt::KeyboardModifiers mods = event.modifiers();
  const bool is_erase = key == Qt::Key_Delete || key == Qt::Key_Backspace;
  const bool is_plain = mods == Qt::NoModifier;
  const bool is_shift = mods == Qt::ShiftModifier;
  const bool is_duplicate =
      key == Qt::Key_D && mods == Qt::ControlModifier;
  return is_erase && is_plain                        ? Command::kRemove
         : is_erase && is_shift                      ? Command::kRipple
         : key == Qt::Key_S && is_plain              ? Command::kSplit
         : event.matches(QKeySequence::Copy)        ? Command::kCopy
         : event.matches(QKeySequence::Cut)         ? Command::kCut
         : event.matches(QKeySequence::Paste)       ? Command::kPaste
         : is_duplicate                              ? Command::kDuplicate
                                                     : Command::kNone;
}

QString ReelTimeline::WhyNot(Command command) const {
  assert(managers_.reel != nullptr);
  assert(command != Command::kNone);
  const Frame now = managers_.playback->FrameOn(Timeline::kReel);
  const std::vector<ClipId> picked = Picked();
  switch (command) {
    case Command::kSplit:
      return managers_.reel->WhyNoSplit(picked, now);
    case Command::kPaste:
      return managers_.reel->WhyNoPaste(now);
    case Command::kDuplicate:
      return managers_.reel->WhyNoDuplicate(picked);
    case Command::kRemove:
    case Command::kRipple:
    case Command::kCopy:
    case Command::kCut:
    case Command::kNone:
      break;
  }
  return managers_.reel->WhyNoPick(picked);
}

void ReelTimeline::Run(Command command) {
  assert(managers_.reel != nullptr);
  assert(command != Command::kNone);
  ReelManager& reel = *managers_.reel;
  const Frame now = managers_.playback->FrameOn(Timeline::kReel);
  const std::vector<ClipId> picked = Picked();
  Result<std::vector<ClipId>> made = std::vector<ClipId>();
  switch (command) {
    case Command::kSplit:
      Report(ProblemOf(reel.SplitAll(picked, now)));
      return;
    case Command::kRemove:
      Report(ProblemOf(reel.RemoveAll(picked)));
      return;
    case Command::kRipple:
      Report(ProblemOf(reel.RippleDeleteAll(picked)));
      return;
    case Command::kCopy:
      Report(ProblemOf(reel.Copy(picked)));
      return;
    case Command::kCut:
      Report(ProblemOf(reel.CutAll(picked)));
      return;
    case Command::kPaste:
      made = reel.Paste(now);
      break;
    case Command::kDuplicate:
      made = reel.DuplicateAll(picked);
      break;
    case Command::kNone:
      return;
  }
  Report(ProblemOf(made));
  if (made) {
    // What was just laid down is what the next edit works on.
    managers_.selection->PickClips(
        std::set<ClipId>(made->begin(), made->end()), PickMode::kReplace);
  }
}

void ReelTimeline::AddCommands(QMenu* menu) {
  assert(menu != nullptr);
  assert(managers_.reel != nullptr);
  struct Entry final {
    Command command;
    const char* name;
    QKeySequence keys;
  };
  const std::array<Entry, 7> entries = {{
      {Command::kSplit, "Split at playhead", QKeySequence(Qt::Key_S)},
      {Command::kCopy, "Copy", QKeySequence::Copy},
      {Command::kCut, "Cut", QKeySequence::Cut},
      {Command::kPaste, "Paste at playhead", QKeySequence::Paste},
      {Command::kDuplicate, "Duplicate", QKeySequence(Qt::CTRL | Qt::Key_D)},
      {Command::kRemove, "Remove", QKeySequence(Qt::Key_Delete)},
      {Command::kRipple, "Ripple remove",
       QKeySequence(Qt::SHIFT | Qt::Key_Delete)},
  }};
  for (const Entry& entry : entries) {
    QAction* action = menu->addAction(tr(entry.name));
    action->setShortcut(entry.keys);
    Explain(action, WhyNot(entry.command));
    const Command command = entry.command;
    connect(action, &QAction::triggered, this,
            [this, command] { Run(command); });
  }
}

void ReelTimeline::contextMenuEvent(QContextMenuEvent* event) {
  assert(event != nullptr);
  assert(managers_.selection != nullptr);
  const ClipId clip = ClipAt(event->pos());
  const bool is_on_clip = clip.IsValid();
  if (is_on_clip) {
    // Right-clicking outside the pick picks just that clip first.
    const bool is_picked = managers_.selection->clips().contains(clip);
    if (!is_picked) {
      managers_.selection->PickClips({clip}, PickMode::kReplace);
    }
    ClipMenu(event->globalPos());
    return;
  }
  const int track = TrackAt(event->pos().y());
  const bool is_on_track = track >= 0;
  if (is_on_track) {
    TrackMenu(track, event->globalPos());
  }
}

void ReelTimeline::ClipMenu(QPoint where) {
  assert(managers_.reel != nullptr);
  assert(!managers_.selection->clips().empty());
  QMenu menu(this);
  menu.setToolTipsVisible(true);
  AddCommands(&menu);
  menu.exec(where);
}

void ReelTimeline::TrackMenu(int track, QPoint where) {
  assert(track >= 0);
  assert(managers_.reel != nullptr);
  QMenu menu(this);
  menu.setToolTipsVisible(true);
  AddCommands(&menu);
  menu.addSeparator();
  QAction* add = menu.addAction(tr("Add track on top"));
  Explain(add, managers_.reel->WhyNoAddTrack());
  connect(add, &QAction::triggered, this,
          [this] { Report(ProblemOf(managers_.reel->AddTrack())); });
  QAction* remove = menu.addAction(tr("Remove track V%1").arg(track + 1));
  Explain(remove, managers_.reel->WhyNoRemoveTrack(track));
  connect(remove, &QAction::triggered, this, [this, track] {
    Report(ProblemOf(managers_.reel->RemoveTrack(track)));
  });
  menu.exec(where);
}

void ReelTimeline::CancelOrClear() {
  assert(managers_.selection != nullptr);
  assert(drag_ != Drag::kSeek || scope_ == nullptr);
  const bool is_editing = scope_ != nullptr;
  const bool is_boxing = drag_ == Drag::kBox;
  if (is_editing) {
    scope_->Cancel();
    scope_.reset();
  } else if (!is_boxing) {
    managers_.selection->PickClips({}, PickMode::kReplace);
  }
  drag_ = Drag::kNone;
  update();
}

void ReelTimeline::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(managers_.reel != nullptr);
  const bool is_escape = event->key() == Qt::Key_Escape;
  const bool is_all = event->matches(QKeySequence::SelectAll);
  const Command command = CommandFor(*event);
  const bool is_command = command != Command::kNone;
  if (is_escape) {
    CancelOrClear();
  } else if (is_all) {
    managers_.selection->PickClips(
        AllClips(managers_.history->current().reel), PickMode::kReplace);
  } else if (is_command) {
    // A key that can't act says why, as its greyed menu entry does.
    const QString why_not = WhyNot(command);
    const bool can_run = why_not.isEmpty();
    if (can_run) {
      Run(command);
    } else {
      Report(why_not);
    }
  } else {
    QWidget::keyPressEvent(event);
  }
}

}  // namespace snapper
