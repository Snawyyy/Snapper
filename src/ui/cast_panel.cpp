#include "ui/cast_panel.h"

#include <cassert>
#include <variant>

#include "edit/doll_library_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"

namespace snapper {
CastPanel::CastPanel(const Managers& managers)
    : managers_(managers),
      layout_(this),
      library_title_(tr("Doll library")),
      import_(tr("Import")),
      reload_(tr("Reload")),
      place_(tr("Put on stage")),
      layers_title_(tr("Layers (top first)")),
      add_menu_(tr("Add")),
      copy_(tr("Copy")),
      remove_(tr("Delete")),
      up_(tr("Up")),
      down_(tr("Down")) {
  assert(managers_.IsComplete());
  BuildLayout();
  Wire();
  RefreshLibrary();
  RefreshLayers();
  assert(library_.parent() != nullptr);
}

void CastPanel::BuildLayout() {
  assert(layout_.count() == 0);
  layout_.setContentsMargins(4, 4, 4, 4);
  layout_.setSpacing(3);
  library_buttons_.addWidget(&import_);
  library_buttons_.addWidget(&reload_);
  library_buttons_.addWidget(&place_);
  add_.setText(tr("Add"));
  add_.setMenu(&add_menu_);
  add_.setPopupMode(QToolButton::InstantPopup);
  for (QWidget* button : {static_cast<QWidget*>(&add_),
                          static_cast<QWidget*>(&copy_),
                          static_cast<QWidget*>(&remove_),
                          static_cast<QWidget*>(&up_),
                          static_cast<QWidget*>(&down_)}) {
    layer_buttons_.addWidget(button);
  }
  layout_.addWidget(&library_title_);
  layout_.addWidget(&library_, 1);
  layout_.addLayout(&library_buttons_);
  layout_.addWidget(&layers_title_);
  layout_.addWidget(&layers_, 2);
  layout_.addLayout(&layer_buttons_);
  // Shift picks a run, Ctrl adds or drops one, as in any list.
  layers_.setSelectionMode(QAbstractItemView::ExtendedSelection);
  library_.setSelectionMode(QAbstractItemView::ExtendedSelection);
  assert(layout_.count() > 0);
}

void CastPanel::Wire() {
  assert(managers_.IsComplete());
  assert(add_.menu() == &add_menu_);
  add_menu_.addAction(tr("Picture..."), this, &CastPanel::AddPicture);
  add_menu_.addAction(tr("Text..."), this, &CastPanel::AddText);
  QMenu* effects = add_menu_.addMenu(tr("Effect"));
  for (int kind = 0; kind < kEffectKindCount; ++kind) {
    const auto effect = static_cast<EffectKind>(kind);
    effects->addAction(EffectName(effect), this, [this, effect] {
      Report(ProblemOf(
          managers_.stage->AddEffect(managers_.selection->shot(), effect)));
    });
  }
  connect(&import_, &QPushButton::clicked, this, &CastPanel::Import);
  connect(&reload_, &QPushButton::clicked, this, &CastPanel::Reload);
  connect(&place_, &QPushButton::clicked, this, &CastPanel::Place);
  connect(&copy_, &QPushButton::clicked, this, [this] {
    Report(ProblemOf(managers_.stage->DuplicateAll(
        managers_.selection->shot(), managers_.selection->PickedLayers())));
  });
  connect(&remove_, &QPushButton::clicked, this, [this] {
    Report(ProblemOf(managers_.stage->RemoveAll(
        managers_.selection->shot(), managers_.selection->PickedLayers())));
  });
  connect(&up_, &QPushButton::clicked, this, [this] { Restack(1); });
  connect(&down_, &QPushButton::clicked, this, [this] { Restack(-1); });
  connect(&library_, &QListWidget::itemSelectionChanged, this,
          &CastPanel::RefreshButtons);
  connect(&layers_, &QListWidget::itemSelectionChanged, this,
          &CastPanel::PickLayer);
  connect(&layers_, &QListWidget::itemChanged, this, &CastPanel::ToggleShown);
  connect(managers_.library, &DollLibraryManager::LibraryChanged, this,
          &CastPanel::RefreshLibrary);
  connect(managers_.library, &DollLibraryManager::ArtChanged, this,
          [this](const QString& name) {
            stale_.insert(name);
            RefreshLibrary();
          });
  connect(managers_.history, &HistoryManager::Changed, this, [this] {
    RefreshLibrary();
    RefreshLayers();
  });
  connect(managers_.selection, &SelectionManager::Changed, this,
          &CastPanel::RefreshLayers);
}

}  // namespace snapper
