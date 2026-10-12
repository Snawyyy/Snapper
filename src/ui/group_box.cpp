#include "ui/group_box.h"

#include <QAction>
#include <QFont>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMenu>

#include <algorithm>
#include <cassert>
#include <memory>

#include "edit/group_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/chip_button.h"
#include "ui/follow.h"
#include "ui/group_chips.h"
#include "ui/problem.h"

namespace snapper {
namespace {

constexpr int kChipColumns = 2;

}  // namespace

GroupBox::GroupBox(const Managers& managers)
    : QGroupBox(tr("Groups")),
      managers_(managers),
      layout_(this),
      chips_layout_(&chips_) {
  assert(managers_.IsComplete());
  chips_layout_.setContentsMargins(0, 0, 0, 0);
  hint_.setWordWrap(true);
  hint_.setText(tr("No groups on what is picked. Pick the dots that should "
                   "behave alike, then press Group."));
  hint_.setObjectName("group_hint");
  make_.setText(tr("Group picked dots"));
  make_.setObjectName("make_group");
  layout_.addWidget(&hint_);
  layout_.addWidget(&chips_);
  layout_.addWidget(&make_);
  connect(&make_, &QPushButton::clicked, this, [this] { AskName(); });
  Follow(managers_, this);
  Refresh();
  assert(layout_.count() == 3);
}

std::set<WarpDot> GroupBox::PickedDots() const {
  assert(managers_.selection != nullptr);
  const std::vector<WarpDot> picked = managers_.selection->dots();
  assert(picked.size() < 10000000);
  return std::set<WarpDot>(picked.begin(), picked.end());
}

void GroupBox::Rebuild(const std::vector<ShownGroup>& shown) {
  assert(shown.size() <= kMaxGroupChips);
  assert(buttons_.size() <= kMaxGroupChips);
  // The old chips may be the one being clicked: let them go after.
  for (ChipButton* button : buttons_) {
    chips_layout_.removeWidget(button);
    button->hide();
    button->deleteLater();
  }
  buttons_.clear();
  names_.clear();
  for (const ShownGroup& group : shown) {
    // The chips_ parent owns the chip from here.
    ChipButton* chip = std::make_unique<ChipButton>(&chips_).release();
    const QString name = group.name;
    chip->setCheckable(true);
    chip->setContextMenuPolicy(Qt::CustomContextMenu);
    chip->setObjectName("group_chip");
    connect(chip, &QPushButton::clicked, this,
            [this, name] { PickChip(name); });
    connect(chip, &QPushButton::customContextMenuRequested, this,
            [this, name, chip](const QPoint& at) {
              ShowMenu(name, chip->mapToGlobal(at));
            });
    const int at = static_cast<int>(buttons_.size());
    chips_layout_.addWidget(chip, at / kChipColumns, at % kChipColumns);
    buttons_.push_back(chip);
    names_.emplace_back(group.name, group.is_default);
  }
}

void GroupBox::Refresh() {
  assert(managers_.groups != nullptr);
  assert(managers_.selection != nullptr);
  const ShotId shot = managers_.selection->shot();
  const std::vector<ShownGroup> shown = GroupsOfPicks(
      managers_.groups->GroupsOf(shot), managers_.selection->picks());
  Names names;
  for (const ShownGroup& group : shown) {
    names.emplace_back(group.name, group.is_default);
  }
  const bool is_changed = names != names_;
  if (is_changed) {
    Rebuild(shown);
  }
  const std::vector<WarpDot> picked = managers_.selection->dots();
  for (size_t i = 0; i < shown.size() && i < buttons_.size(); ++i) {
    const ChipLook look = ChipOf(shown[i], picked);
    QFont font = buttons_[i]->font();
    font.setItalic(shown[i].is_default);
    font.setBold(look.share == GroupShare::kSome);
    buttons_[i]->setFont(font);
    buttons_[i]->SetFullText(look.text);
    buttons_[i]->setToolTip(look.tip);
    buttons_[i]->setChecked(look.share == GroupShare::kAll);
  }
  hint_.setVisible(shown.empty());
  const QString why = managers_.groups->WhyNoGroup(shot, PickedDots());
  const bool can_group = why.isEmpty();
  make_.setEnabled(can_group);
  make_.setToolTip(can_group ? tr("Make a group of the %1 picked dots, to "
                                  "pick them all with one click.")
                                   .arg(picked.size())
                             : why);
}

void GroupBox::PickChip(const QString& name) {
  assert(managers_.groups != nullptr);
  assert(managers_.selection != nullptr);
  const Qt::KeyboardModifiers keys = QGuiApplication::keyboardModifiers();
  const bool is_shift = keys.testFlag(Qt::ShiftModifier);
  const bool is_ctrl = keys.testFlag(Qt::ControlModifier);
  const ShotId shot = managers_.selection->shot();
  for (const ShownGroup& group : managers_.groups->GroupsOf(shot)) {
    const bool is_it = group.name == name;
    if (is_it) {
      managers_.selection->PickGroup(group.dots,
                                     GroupPickMode(is_shift, is_ctrl));
    }
  }
  // A click flips a pressed chip by itself; the pick says how it looks.
  Refresh();
}

void GroupBox::AskName() {
  assert(managers_.groups != nullptr);
  assert(managers_.selection != nullptr);
  const ShotId shot = managers_.selection->shot();
  bool is_asked = false;
  const QString name = QInputDialog::getText(
      this, tr("New group"), tr("Name the group"), QLineEdit::Normal,
      managers_.groups->SuggestedName(shot), &is_asked);
  if (is_asked) {
    emit Problem(ProblemOf(
        managers_.groups->Create(shot, PickedDots(), name)));
  }
}

void GroupBox::AskRename(const QString& name) {
  assert(managers_.groups != nullptr);
  assert(managers_.selection != nullptr);
  bool is_asked = false;
  const QString to =
      QInputDialog::getText(this, tr("Rename group"), tr("New name"),
                            QLineEdit::Normal, name, &is_asked);
  if (is_asked) {
    emit Problem(ProblemOf(managers_.groups->Rename(
        managers_.selection->shot(), name, to)));
  }
}

void GroupBox::ShowMenu(const QString& name, const QPoint& at) {
  assert(managers_.groups != nullptr);
  assert(managers_.selection != nullptr);
  const ShotId shot = managers_.selection->shot();
  const auto found = std::ranges::find(names_, name,
                                       &Names::value_type::first);
  const bool is_known = found != names_.end();
  if (!is_known) {
    return;
  }
  const bool is_default = found->second;
  const std::set<WarpDot> picked = PickedDots();
  const bool has_picked = !picked.empty();
  GroupManager* groups = managers_.groups;
  QMenu menu;
  menu.setToolTipsVisible(true);
  menu.addAction(tr("Rename..."), this, [this, name] { AskRename(name); });
  for (const bool is_add : {true, false}) {
    QAction* action = menu.addAction(
        is_add ? tr("Add picked dots") : tr("Remove picked dots"), this,
        [this, groups, shot, name, picked, is_add] {
          emit Problem(ProblemOf(
              is_add ? groups->AddDots(shot, name, picked)
                     : groups->RemoveDots(shot, name, picked)));
        });
    action->setEnabled(has_picked);
    action->setToolTip(has_picked ? QString() : tr("Pick some dots first."));
  }
  menu.addAction(
      is_default ? tr("Make only this shot's")
                 : tr("Make a default for every shot of this doll"),
      this, [this, groups, shot, name, is_default] {
        emit Problem(
            ProblemOf(groups->SetDefault(shot, name, !is_default)));
      });
  menu.addAction(tr("Delete group"), this, [this, groups, shot, name] {
    emit Problem(ProblemOf(groups->Delete(shot, name)));
  });
  menu.exec(at);
}

}  // namespace snapper
