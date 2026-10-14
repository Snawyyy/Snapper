#include "ui/comp_effects.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QMenu>

#include <cassert>

#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/comp_panel.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/problem.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The margin inside an open fold, past the indent on the left.
constexpr int kBodyMargin = 4;

// A flat button on an effect's plate.
QToolButton* PlateButton(const QString& tip, QWidget* plate) {
  assert(plate != nullptr);
  assert(!tip.isEmpty());
  auto* button = MakeOwned<QToolButton>(plate);
  button->setAutoRaise(true);
  button->setFocusPolicy(Qt::NoFocus);
  button->setToolTip(tip);
  QFont bold = button->font();
  bold.setBold(true);
  button->setFont(bold);
  return button;
}

}  // namespace

CompEffects::CompEffects(const Managers& managers)
    : managers_(managers), layout_(this), light_layout_(&light_body_),
      light_(managers) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  layout_.setContentsMargins(0, 0, 0, 0);
  layout_.setSpacing(theme::kRowGap);
  title_.setText(tr("Screen"));
  MakeTitlePlate(&title_);
  layout_.addWidget(&title_);
  folds_ = MakeOwned<QWidget>(this);
  layout_.addWidget(folds_);
  light_layout_.setContentsMargins(0, 0, 0, 0);
  light_layout_.addWidget(&light_);
  layout_.addWidget(PlatedSection(tr("Shot light"), &light_body_, this));
  layout_.addStretch();
  connect(&light_, &CompLight::Problem, this, &CompEffects::Problem);
  const auto refresh = [this] { Refresh(); };
  connect(managers_.history, &HistoryManager::Changed, this, refresh);
  connect(managers_.comp, &CompManager::Changed, this, refresh);
  connect(managers_.selection, &SelectionManager::Changed, this, refresh);
  Refresh();
}

std::vector<CompEffect> CompEffects::Shown() const {
  assert(managers_.comp != nullptr);
  assert(managers_.selection != nullptr);
  return managers_.comp->ShownFor(managers_.selection->shot(),
                                  managers_.selection->layer(),
                                  managers_.selection->pieces());
}

QString CompEffects::Signature(const std::vector<CompEffect>& shown) const {
  assert(managers_.selection != nullptr);
  assert(shown.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  QString signature = QStringLiteral("%1/%2")
                          .arg(managers_.selection->shot().value())
                          .arg(managers_.selection->layer().value());
  for (const QString& piece : managers_.selection->pieces()) {
    signature += QStringLiteral("/") + piece;
  }
  for (const CompEffect& effect : shown) {
    signature += QStringLiteral("e%1").arg(effect.id.value());
  }
  return signature;
}

void CompEffects::Refresh() {
  assert(managers_.comp != nullptr);
  assert(managers_.selection != nullptr);
  const ShotId shot = managers_.selection->shot();
  CompTarget picked;
  picked.layer = managers_.selection->layer();
  for (const QString& piece : managers_.selection->pieces()) {
    picked.pieces.push_back(piece);
  }
  title_.setText(managers_.comp->TargetText(shot, picked));
  const std::vector<CompEffect> shown = Shown();
  const QString signature = Signature(shown);
  const bool is_new = signature != signature_;
  if (is_new) {
    signature_ = signature;
    Rebuild(shown);
  }
  ShowPlates();
}

void CompEffects::Rebuild(const std::vector<CompEffect>& shown) {
  assert(folds_ != nullptr);
  assert(shown.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  plates_.clear();
  const int at = layout_.indexOf(folds_);
  folds_->hide();
  folds_->deleteLater();
  folds_ = MakeOwned<QWidget>(this);
  layout_.insertWidget(at, folds_);
  auto* column = MakeOwned<QVBoxLayout>(folds_);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(theme::kRowGap);
  const bool is_empty = shown.empty();
  if (is_empty) {
    column->addWidget(EmptyHint(
        tr("No effects here. Double click one on the left to add it."),
        folds_));
  }
  for (const CompEffect& effect : shown) {
    AddFold(effect, column);
  }
}

void CompEffects::AddFold(const CompEffect& effect, QVBoxLayout* into) {
  assert(into != nullptr);
  assert(effect.id.IsValid());
  const ShotId shot = managers_.selection->shot();
  const CompEffectId id = effect.id;
  const std::pair<int, int> key{shot.value(), id.value()};
  auto* plate = MakeOwned<QFrame>(folds_);
  plate->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
  plate->setContextMenuPolicy(Qt::CustomContextMenu);
  plate->setToolTip(QStringLiteral("%1\n%2").arg(
      CompManager::EffectHelp(effect),
      tr("Works on: %1. Right click to move, duplicate or remove.")
          .arg(managers_.comp->TargetText(shot, effect.target))));
  auto* row = MakeOwned<QHBoxLayout>(plate);
  row->setContentsMargins(2, 2, 4, 2);
  QToolButton* arrow = PlateButton(tr("Show or hide its settings."), plate);
  auto* eye = MakeOwned<QCheckBox>(plate);
  eye->setFocusPolicy(Qt::NoFocus);
  eye->setToolTip(tr("Turn the effect on or off."));
  QToolButton* name = PlateButton(tr("Pick it, for K, I and Delete."), plate);
  name->setText(CompManager::EffectName(effect));
  name->setCheckable(true);
  name->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto* slow = MakeOwned<QLabel>(tr("slow"), plate);
  slow->setStyleSheet(QStringLiteral("QLabel { color: #f2c230; }"));
  slow->setToolTip(tr("It took longer than a frame at 24 fps."));
  row->addWidget(arrow);
  row->addWidget(eye);
  row->addWidget(name, 1);
  row->addWidget(slow);
  into->addWidget(plate);
  QFrame* body = NewBody(id);
  into->addWidget(body);
  const bool is_open = !closed_.contains(key);
  body->setVisible(is_open);
  arrow->setArrowType(is_open ? Qt::DownArrow : Qt::RightArrow);
  connect(arrow, &QToolButton::clicked, this, [this, key, arrow, body] {
    const bool was_open = body->isVisible();
    body->setVisible(!was_open);
    arrow->setArrowType(was_open ? Qt::RightArrow : Qt::DownArrow);
    if (was_open) {
      closed_.insert(key);
    } else {
      closed_.erase(key);
    }
  });
  connect(eye, &QCheckBox::clicked, this, [this, id](bool is_on) {
    Report(ProblemOf(
        managers_.comp->SetOn(managers_.selection->shot(), id, is_on)));
  });
  connect(name, &QToolButton::clicked, this, [this, id] {
    managers_.comp->Pick(managers_.selection->shot(), id);
    ShowPlates();
  });
  connect(plate, &QWidget::customContextMenuRequested, this,
          [this, id, plate](const QPoint& at) {
            OpenMenu(id, plate->mapToGlobal(at));
          });
  plates_.push_back(Plate{id, eye, name, slow});
}

QFrame* CompEffects::NewBody(CompEffectId id) {
  assert(id.IsValid());
  assert(folds_ != nullptr);
  auto* body = MakeOwned<QFrame>(folds_);
  body->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  auto* inside = MakeOwned<QVBoxLayout>(body);
  inside->setContentsMargins(theme::kIndent, kBodyMargin, kBodyMargin,
                             kBodyMargin);
  auto* panel = MakeOwned<CompPanel>(managers_, id, body);
  inside->addWidget(panel);
  connect(panel, &CompPanel::Problem, this, &CompEffects::Problem);
  return body;
}

void CompEffects::ShowPlates() {
  assert(managers_.comp != nullptr);
  assert(plates_.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  const ShotId shot = managers_.selection->shot();
  const CompEffectId picked = managers_.comp->PickedIn(shot);
  for (const Plate& plate : plates_) {
    const CompEffect* effect = managers_.comp->Find(shot, plate.id);
    const bool is_gone = effect == nullptr;
    if (is_gone) {
      continue;
    }
    plate.eye->setChecked(effect->is_on);
    plate.name->setChecked(plate.id == picked);
    plate.slow->setVisible(managers_.comp->IsSlow(plate.id));
  }
}

void CompEffects::OpenMenu(CompEffectId id, const QPoint& at) {
  assert(id.IsValid());
  assert(managers_.comp != nullptr);
  const ShotId shot = managers_.selection->shot();
  managers_.comp->Pick(shot, id);
  const std::vector<CompEffect> shown = Shown();
  const int index = CompEffectIndex(shown, id);
  const int last = static_cast<int>(shown.size()) - 1;
  QMenu menu;
  QAction* up = menu.addAction(tr("Move up"));
  QAction* down = menu.addAction(tr("Move down"));
  menu.addSeparator();
  QAction* duplicate = menu.addAction(tr("Duplicate\tCtrl+D"));
  QAction* remove = menu.addAction(tr("Remove\tDelete"));
  Explain(up, index <= 0 ? tr("It already runs first.") : QString());
  Explain(down, index >= last ? tr("It already runs last.") : QString());
  Explain(duplicate, managers_.comp->WhyNoAdd(shot));
  const QAction* chosen = menu.exec(at);
  const bool is_up = chosen == up;
  const bool is_down = chosen == down;
  const bool is_duplicate = chosen == duplicate;
  const bool is_remove = chosen == remove;
  const bool is_move = is_up || is_down;
  if (is_move) {
    Step(id, is_up ? -1 : 1);
  } else if (is_duplicate) {
    Report(ProblemOf(managers_.comp->Duplicate(shot, id)));
  } else if (is_remove) {
    Report(ProblemOf(managers_.comp->Remove(shot, id)));
  }
}

void CompEffects::Step(CompEffectId id, int direction) {
  assert(direction == -1 || direction == 1);
  assert(id.IsValid());
  const ShotId shot = managers_.selection->shot();
  const std::vector<CompEffect> shown = Shown();
  const int index = CompEffectIndex(shown, id);
  const int next = index + direction;
  const bool is_inside =
      index >= 0 && next >= 0 && next < static_cast<int>(shown.size());
  if (!is_inside) {
    return;
  }
  // It takes its listed neighbour's place in the whole stack.
  const int to = CompEffectIndex(managers_.comp->Of(shot),
                                 shown[static_cast<size_t>(next)].id);
  Report(ProblemOf(managers_.comp->Move(shot, id, to)));
}

}  // namespace snapper
