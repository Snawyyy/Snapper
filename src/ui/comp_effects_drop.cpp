// CompEffects: clicks and drags on the plates, drops from the library,
// and the right click menus.

#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QMenu>
#include <QMouseEvent>

#include <cassert>

#include "edit/selection_manager.h"
#include "ui/comp_drag.h"
#include "ui/comp_effects.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/problem.h"
#include "ui/theme.h"

namespace snapper {

const CompEffects::Plate* CompEffects::PlateOf(const QObject* widget) const {
  assert(widget != nullptr);
  assert(plates_.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  for (const Plate& plate : plates_) {
    const bool is_it = plate.plate == widget || plate.name == widget;
    if (is_it) {
      return &plate;
    }
  }
  return nullptr;
}

bool CompEffects::eventFilter(QObject* watched, QEvent* event) {
  assert(watched != nullptr);
  assert(event != nullptr);
  const Plate* plate = PlateOf(watched);
  const bool is_plate = plate != nullptr;
  if (!is_plate) {
    return QWidget::eventFilter(watched, event);
  }
  const QEvent::Type type = event->type();
  const auto* mouse = static_cast<const QMouseEvent*>(event);
  const bool is_press = type == QEvent::MouseButtonPress &&
                        mouse->button() == Qt::LeftButton;
  if (is_press) {
    pressed_ = plate->id;
    press_at_ = mouse->globalPosition();
    return true;
  }
  const bool is_held = pressed_ == plate->id;
  const bool is_drag =
      is_held && type == QEvent::MouseMove &&
      (mouse->globalPosition() - press_at_).manhattanLength() >=
          QApplication::startDragDistance();
  if (is_drag) {
    pressed_ = CompEffectId();
    StartDrag(*plate);
    return true;
  }
  const bool is_click = is_held && type == QEvent::MouseButtonRelease;
  if (is_click) {
    pressed_ = CompEffectId();
    managers_.comp->Pick(managers_.selection->shot(), plate->id);
    Toggle(*plate);
    return true;
  }
  return QWidget::eventFilter(watched, event);
}

void CompEffects::StartDrag(const Plate& plate) {
  assert(plate.plate != nullptr);
  assert(plate.id.IsValid());
  const CompEffectId id = plate.id;
  managers_.comp->Pick(managers_.selection->shot(), id);
  auto* drag = MakeOwned<QDrag>(this);
  drag->setMimeData(EffectMime(id).release());
  drag->setPixmap(plate.plate->grab());
  drag->setHotSpot(plate.plate->mapFromGlobal(press_at_).toPoint());
  moving_ = CompEffectId();
  moving_gap_ = -1;
  // Escape cancels; the move waits until the drag is over, as it
  // rebuilds the plate this drag started from.
  drag->exec(Qt::MoveAction);
  drop_line_.hide();
  const bool has_move = moving_.IsValid() && moving_gap_ >= 0;
  if (has_move) {
    MoveToGap(moving_, moving_gap_);
  }
  moving_ = CompEffectId();
  moving_gap_ = -1;
}

int CompEffects::GapAt(double y) const {
  assert(plates_.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  assert(y > -1e9);
  int gap = 0;
  for (const Plate& plate : plates_) {
    const double middle =
        plate.plate->mapTo(this, QPoint(0, plate.plate->height() / 2)).y();
    const bool is_below = y > middle;
    gap += is_below ? 1 : 0;
  }
  return gap;
}

int CompEffects::StackIndexOf(int gap) const {
  assert(gap >= 0);
  assert(managers_.comp != nullptr);
  const std::vector<CompEffect> shown = Shown();
  const std::vector<CompEffect> stack =
      managers_.comp->Of(managers_.selection->shot());
  const int count = static_cast<int>(shown.size());
  const bool is_before = gap < count;
  if (is_before) {
    return CompEffectIndex(stack, shown[static_cast<size_t>(gap)].id);
  }
  const bool is_empty = shown.empty();
  return is_empty ? kOnTop : CompEffectIndex(stack, shown.back().id) + 1;
}

void CompEffects::ShowDropLine(int gap) {
  assert(gap >= 0);
  assert(gap <= static_cast<int>(plates_.size()));
  const int count = static_cast<int>(plates_.size());
  const bool is_empty = count == 0;
  int y = folds_->mapTo(this, QPoint(0, 0)).y();
  if (!is_empty) {
    const bool is_before = gap < count;
    const Plate& near = plates_[static_cast<size_t>(is_before ? gap
                                                              : count - 1)];
    const QWidget* edge = !is_before && near.body->isVisible() ? near.body
                                                               : near.plate;
    y = is_before ? edge->mapTo(this, QPoint(0, 0)).y() - theme::kRowGap / 2
                  : edge->mapTo(this, QPoint(0, edge->height())).y() +
                        theme::kRowGap / 2;
  }
  drop_line_.setGeometry(0, y - 1, width(), 3);
  drop_line_.raise();
  drop_line_.show();
}

void CompEffects::MoveToGap(CompEffectId id, int gap) {
  assert(id.IsValid());
  assert(gap >= 0);
  const ShotId shot = managers_.selection->shot();
  const std::vector<CompEffect> shown = Shown();
  const int from = CompEffectIndex(shown, id);
  const bool is_still = from < 0 || gap == from || gap == from + 1 ||
                        gap > static_cast<int>(shown.size());
  if (is_still) {
    return;
  }
  // It takes its listed neighbour's place in the whole stack.
  const CompEffect& near =
      shown[static_cast<size_t>(gap > from ? gap - 1 : gap)];
  const int to = CompEffectIndex(managers_.comp->Of(shot), near.id);
  Report(ProblemOf(managers_.comp->Move(shot, id, to)));
}

void CompEffects::dragEnterEvent(QDragEnterEvent* event) {
  assert(event != nullptr);
  const bool is_ours =
      EntryOf(event->mimeData()).has_value() ||
      (EffectOf(event->mimeData()).has_value() && event->source() == this);
  assert(!is_ours || event->mimeData() != nullptr);
  if (is_ours) {
    event->acceptProposedAction();
    ShowDropLine(GapAt(event->position().y()));
  }
}

void CompEffects::dragMoveEvent(QDragMoveEvent* event) {
  assert(event != nullptr);
  assert(event->mimeData() != nullptr);
  event->acceptProposedAction();
  ShowDropLine(GapAt(event->position().y()));
}

void CompEffects::dragLeaveEvent(QDragLeaveEvent* event) {
  assert(event != nullptr);
  assert(drop_line_.parentWidget() == this);
  drop_line_.hide();
}

void CompEffects::dropEvent(QDropEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  drop_line_.hide();
  const int gap = GapAt(event->position().y());
  const auto moved = EffectOf(event->mimeData());
  const bool is_move = moved.has_value() && event->source() == this;
  if (is_move) {
    moving_ = *moved;
    moving_gap_ = gap;
    event->acceptProposedAction();
    return;
  }
  const auto entry = EntryOf(event->mimeData());
  const bool is_entry = entry.has_value();
  if (!is_entry) {
    return;
  }
  event->acceptProposedAction();
  const ShotId shot = managers_.selection->shot();
  const auto target = managers_.comp->TargetFor(
      shot, managers_.selection->layer(), managers_.selection->pieces());
  if (!target) {
    Report(target.error().message);
    return;
  }
  Report(ProblemOf(
      managers_.comp->AddEntry(shot, *entry, *target, StackIndexOf(gap))));
}

void CompEffects::PasteAt(int index) {
  assert(managers_.comp != nullptr);
  assert(index >= kOnTop);
  const ShotId shot = managers_.selection->shot();
  const auto target = managers_.comp->TargetFor(
      shot, managers_.selection->layer(), managers_.selection->pieces());
  if (!target) {
    Report(target.error().message);
    return;
  }
  Report(ProblemOf(managers_.comp->Paste(shot, *target, index)));
}

void CompEffects::Paste() {
  assert(managers_.comp != nullptr);
  assert(managers_.selection != nullptr);
  PasteAt(StackIndexOf(static_cast<int>(Shown().size())));
}

void CompEffects::OpenMenu(CompEffectId id, const QPoint& at) {
  assert(id.IsValid());
  assert(managers_.comp != nullptr);
  CompManager* comp = managers_.comp;
  const ShotId shot = managers_.selection->shot();
  comp->Pick(shot, id);
  const std::vector<CompEffect> shown = Shown();
  const int index = CompEffectIndex(shown, id);
  const int last = static_cast<int>(shown.size()) - 1;
  const QString copied = comp->CopiedName();
  QMenu menu;
  QAction* copy = menu.addAction(tr("Copy\tCtrl+C"));
  QAction* paste = menu.addAction(
      copied.isEmpty() ? tr("Paste below\tCtrl+V")
                       : tr("Paste %1 below\tCtrl+V").arg(copied));
  QAction* duplicate = menu.addAction(tr("Duplicate\tCtrl+D"));
  menu.addSeparator();
  QAction* up = menu.addAction(tr("Move up"));
  QAction* down = menu.addAction(tr("Move down"));
  menu.addSeparator();
  QAction* remove = menu.addAction(tr("Remove\tDelete"));
  Explain(paste, comp->WhyNoPaste(shot));
  Explain(duplicate, comp->WhyNoAdd(shot));
  Explain(up, index <= 0 ? tr("It already runs first.") : QString());
  Explain(down, index >= last ? tr("It already runs last.") : QString());
  const QAction* chosen = menu.exec(at);
  const bool is_move = chosen == up || chosen == down;
  const bool is_copy = chosen == copy;
  const bool is_paste = chosen == paste;
  const bool is_duplicate = chosen == duplicate;
  const bool is_remove = chosen == remove;
  if (is_copy) {
    Report(ProblemOf(comp->Copy(shot, id)));
  } else if (is_paste) {
    PasteAt(CompEffectIndex(comp->Of(shot), id) + 1);
  } else if (is_duplicate) {
    Report(ProblemOf(comp->Duplicate(shot, id)));
  } else if (is_move) {
    MoveToGap(id, chosen == up ? index - 1 : index + 2);
  } else if (is_remove) {
    Report(ProblemOf(comp->Remove(shot, id)));
  }
}

void CompEffects::contextMenuEvent(QContextMenuEvent* event) {
  assert(event != nullptr);
  assert(managers_.comp != nullptr);
  const ShotId shot = managers_.selection->shot();
  const QString copied = managers_.comp->CopiedName();
  QMenu menu;
  QAction* paste =
      menu.addAction(copied.isEmpty() ? tr("Paste\tCtrl+V")
                                      : tr("Paste %1\tCtrl+V").arg(copied));
  Explain(paste, managers_.comp->WhyNoPaste(shot));
  const bool is_paste = menu.exec(event->globalPos()) == paste;
  if (is_paste) {
    Paste();
  }
}

}  // namespace snapper
