#include "ui/skin_rig_box.h"

#include <QInputDialog>
#include <QMenu>

#include <cassert>
#include <memory>
#include <vector>

#include "edit/history_manager.h"
#include "edit/skin_manager.h"
#include "ui/form_helpers.h"
#include "ui/hover_fade.h"
#include "ui/plated_box.h"
#include "ui/problem.h"
#include "ui/toast_manager.h"

namespace snapper {

SkinRigBox::SkinRigBox(const Managers& managers)
    : managers_(managers),
      tiles_(tr("Click to pull this tape out and change the skin: pick "
                "pieces and set their drawing below. Right click for "
                "more.")),
      add_(tr("+ Blank tape")),
      from_krita_(tr("Auto-load from Krita")) {
  assert(managers_.IsComplete());
  assert(managers_.skins != nullptr);
  PlateBox(this, tr("Skins"), &layout_);
  add_.setToolTip(tr("A new skin: a copy of the one being changed, or one "
                     "that changes nothing yet."));
  hint_.setWordWrap(true);
  layout_.addWidget(&tiles_);
  // Deck keys, one even row: record a blank tape, or load one per
  // "skin/drawing" name the Krita file already has.
  for (TapeKey* key : {&add_, &from_krita_}) {
    key->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // The key owns it; it sinks the key as it is pressed.
    std::make_unique<HoverFade>(key, managers_.motion).release();
    buttons_.addWidget(key, 1);
  }
  layout_.addLayout(&buttons_);
  layout_.addWidget(&hint_);
  connect(&tiles_, &SkinTiles::Clicked, this,
          [this](const QString& skin) { Pick(skin == skin_ ? QString() : skin); });
  connect(&tiles_, &SkinTiles::MenuAsked, this, &SkinRigBox::ShowMenu);
  connect(&add_, &QPushButton::clicked, this, [this] {
    const auto added = managers_.skins->AddSkin(doll_, skin_);
    emit Problem(ProblemOf(added));
    if (added.has_value()) {
      Pick(*added);
    }
  });
  connect(&from_krita_, &QPushButton::clicked, this, [this] {
    const auto made = managers_.skins->MakeFromKritaNames(doll_);
    emit Problem(ProblemOf(made));
    if (made.has_value()) {
      managers_.toasts->Cheer(tr("Made %n skin(s) from Krita names.", "",
                                 *made));
    }
  });
  connect(managers_.history, &HistoryManager::Changed, this,
          &SkinRigBox::Refresh);
  Refresh();
}

void SkinRigBox::SetDoll(const QString& doll) {
  assert(doll.size() < 100000);
  const bool is_new = doll != doll_;
  doll_ = doll;
  if (is_new) {
    Pick(QString());
  }
  Refresh();
}

void SkinRigBox::Pick(const QString& skin) {
  assert(skin.size() < 100000);
  const bool is_new = skin != skin_;
  skin_ = skin;
  if (is_new) {
    emit SkinPicked(skin_);
  }
  Refresh();
}

void SkinRigBox::Refresh() {
  assert(managers_.skins != nullptr);
  const std::vector<Skin> skins = managers_.skins->SkinsOf(doll_);
  std::vector<QString> names;
  bool has_picked = false;
  for (const Skin& skin : skins) {
    names.push_back(skin.name);
    has_picked = has_picked || skin.name == skin_;
  }
  const bool is_gone = !skin_.isEmpty() && !has_picked;
  if (is_gone) {
    skin_.clear();
    emit SkinPicked(skin_);
  }
  tiles_.Show(names, skin_.isEmpty() ? std::vector<QString>()
                                     : std::vector<QString>{skin_});
  Explain(&add_, doll_.isEmpty() ? tr("Import a doll first.") : QString());
  Explain(&from_krita_, managers_.skins->WhyNoKritaSkins(doll_));
  hint_.setText(
      skin_.isEmpty()
          ? tr("A skin swaps drawings across many pieces at once. Pull a "
               "tape out to change it, or record a blank one.")
          : tr("Changing %1: pick pieces and set their Drawing. Push the "
               "tape back in when done.")
                .arg(skin_));
  setEnabled(!doll_.isEmpty());
}

void SkinRigBox::ShowMenu(const QString& skin, QPoint global) {
  assert(!skin.isEmpty());
  assert(managers_.skins != nullptr);
  SkinManager* skins = managers_.skins;
  const QString doll = doll_;
  QMenu menu(this);
  menu.addSection(skin);
  menu.addAction(tr("Rename..."), this, [=, this] {
    bool is_ok = false;
    const QString to = QInputDialog::getText(
        this, tr("Rename skin"), tr("Name"), QLineEdit::Normal, skin, &is_ok);
    if (is_ok) {
      const QString problem = ProblemOf(skins->RenameSkin(doll, skin, to));
      emit Problem(problem);
      const bool is_renamed = problem.isEmpty() && skin_ == skin;
      if (is_renamed) {
        Pick(to.trimmed());
      }
    }
  });
  menu.addAction(tr("Duplicate"), this, [=, this] {
    emit Problem(ProblemOf(skins->AddSkin(doll, skin)));
  });
  menu.addAction(tr("Move up"), this, [=, this] {
    emit Problem(ProblemOf(skins->MoveSkin(doll, skin, -1)));
  });
  menu.addAction(tr("Move down"), this, [=, this] {
    emit Problem(ProblemOf(skins->MoveSkin(doll, skin, 1)));
  });
  QAction* remove = menu.addAction(tr("Delete"), this, [=, this] {
    emit Problem(ProblemOf(skins->RemoveSkin(doll, skin)));
  });
  Explain(remove, skins->WhyNoRemove(doll, skin));
  menu.exec(global);
}

}  // namespace snapper
