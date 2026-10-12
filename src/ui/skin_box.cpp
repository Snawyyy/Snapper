#include "ui/skin_box.h"

#include <QMenu>

#include <cassert>
#include <vector>

#include "edit/selection_manager.h"
#include "edit/skin_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/playhead.h"
#include "ui/problem.h"

namespace snapper {

SkinBox::SkinBox(const Managers& managers)
    : QGroupBox(tr("Skins")),
      managers_(managers),
      layout_(this),
      tiles_(tr("Click to put this skin on at the playhead, again to take it "
                "off; Shift click stacks it on what is worn. Right click for "
                "more.")),
      previous_(this),
      next_(this) {
  assert(managers_.IsComplete());
  assert(managers_.skins != nullptr);
  layout_.addWidget(&tiles_);
  empty_.setWordWrap(true);
  empty_.setText(tr("This doll has no skins. Make them in the Rig tab."));
  layout_.addWidget(&empty_);
  // Shift+[ types a brace on most keyboards; both are bound.
  previous_.setKeys({QKeySequence(Qt::SHIFT | Qt::Key_BracketLeft),
                     QKeySequence(Qt::Key_BraceLeft),
                     QKeySequence(Qt::SHIFT | Qt::Key_BraceLeft)});
  next_.setKeys({QKeySequence(Qt::SHIFT | Qt::Key_BracketRight),
                 QKeySequence(Qt::Key_BraceRight),
                 QKeySequence(Qt::SHIFT | Qt::Key_BraceRight)});
  connect(&previous_, &QShortcut::activated, this, [this] { Step(-1); });
  connect(&next_, &QShortcut::activated, this, [this] { Step(1); });
  connect(&tiles_, &SkinTiles::Clicked, this, &SkinBox::Wear);
  connect(&tiles_, &SkinTiles::MenuAsked, this, &SkinBox::ShowMenu);
  Follow(managers_, this);
  Refresh();
}

void SkinBox::Refresh() {
  assert(managers_.selection != nullptr);
  assert(managers_.skins != nullptr);
  const ShotId shot = managers_.selection->shot();
  const LayerId layer = managers_.selection->layer();
  const QString doll = managers_.skins->DollOf(shot, layer);
  std::vector<QString> names;
  for (const Skin& skin : managers_.skins->SkinsOf(doll)) {
    names.push_back(skin.name);
  }
  const auto spot = SpotOf(managers_);
  const bool is_here = spot.has_value() && spot->shot == shot;
  const std::vector<QString> worn =
      is_here ? managers_.skins->WornAt(shot, layer, spot->local)
              : std::vector<QString>();
  tiles_.Show(names, worn);
  tiles_.setEnabled(is_here);
  tiles_.setToolTip(is_here ? QString()
                            : tr("Move the playhead into this shot."));
  empty_.setVisible(names.empty());
}

void SkinBox::Wear(const QString& skin, Qt::KeyboardModifiers keys) {
  assert(!skin.isEmpty());
  assert(managers_.selection != nullptr);
  const auto spot = SpotOf(managers_);
  const bool is_here =
      spot.has_value() && spot->shot == managers_.selection->shot();
  if (is_here) {
    emit Problem(ProblemOf(managers_.skins->Wear(
        spot->shot, managers_.selection->PickedLayers(),
        managers_.selection->layer(), spot->local, skin,
        keys.testFlag(Qt::ShiftModifier))));
  }
  Refresh();
}

void SkinBox::Step(int by) {
  assert(by == -1 || by == 1);
  assert(managers_.selection != nullptr);
  const auto spot = SpotOf(managers_);
  const bool is_here =
      spot.has_value() && spot->shot == managers_.selection->shot() &&
      !managers_.skins->DollOf(spot->shot, managers_.selection->layer())
           .isEmpty();
  if (is_here) {
    emit Problem(ProblemOf(managers_.skins->Step(
        spot->shot, managers_.selection->PickedLayers(),
        managers_.selection->layer(), spot->local, by)));
  }
}

void SkinBox::ShowMenu(const QString& skin, QPoint global) {
  assert(!skin.isEmpty());
  assert(managers_.selection != nullptr);
  const auto spot = SpotOf(managers_);
  const bool is_here =
      spot.has_value() && spot->shot == managers_.selection->shot();
  QMenu menu(this);
  menu.addSection(skin);
  QAction* update = menu.addAction(tr("Update from current look"), this, [=, this] {
    emit Problem(ProblemOf(managers_.skins->UpdateFromLook(
        spot->shot, managers_.selection->layer(), spot->local, skin)));
  });
  update->setToolTip(tr("Takes the drawings swapped (and pieces hidden) by "
                        "hand at the playhead into this skin."));
  Explain(update, is_here ? QString()
                          : tr("Move the playhead into this shot."));
  menu.exec(global);
}

}  // namespace snapper
