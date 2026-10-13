#include "ui/cast_panel.h"

#include <cassert>
#include <variant>

#include "edit/doll_library_manager.h"
#include "edit/history_manager.h"
#include "edit/library_folder_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"
#include "edit/document_manager.h"
#include "ui/doll_drop.h"
#include "ui/fold_section.h"
#include "ui/library_nav_bar.h"
#include "ui/panel_kit.h"
#include "ui/pick_list.h"
#include "ui/theme.h"
#include "ui/toast_manager.h"
#include "ui/toast_queue.h"

namespace snapper {
CastPanel::CastPanel(const Managers& managers)
    : managers_(managers),
      layout_(this),
      import_(tr("Import")),
      reload_(tr("Reload")),
      place_(tr("Put on stage")),
      add_objects_(tr("Add pictures...")),
      background_(tr("Background...")),
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
  layout_.setSpacing(theme::kRowGap);
  library_buttons_.addWidget(&import_);
  library_buttons_.addWidget(&reload_);
  library_buttons_.addWidget(&place_);
  add_.setText(tr("Add"));
  add_.setMenu(&add_menu_);
  add_menu_.setToolTipsVisible(true);
  add_.setPopupMode(QToolButton::InstantPopup);
  for (QWidget* button : {static_cast<QWidget*>(&add_),
                          static_cast<QWidget*>(&copy_),
                          static_cast<QWidget*>(&remove_),
                          static_cast<QWidget*>(&up_),
                          static_cast<QWidget*>(&down_)}) {
    layer_buttons_.addWidget(button);
  }
  // Library: the dolls, with their buttons under them.
  auto* library_body = MakeOwned<QWidget>(this);
  auto* library_column = MakeOwned<QVBoxLayout>(library_body);
  library_column->setContentsMargins(0, 0, 0, 0);
  const bool has_folders = managers_.library_folders != nullptr;
  if (has_folders) {
    nav_ = MakeOwned<LibraryNavBar>(managers_);
    library_column->addWidget(nav_);
    library_.AcceptDollsOnFolders();
  }
  library_column->addWidget(&library_, 1);
  library_column->addLayout(&library_buttons_);
  QWidget* library = PlatedSection(tr("Library"), library_body, this);
  library_plate_ = library->findChild<QLabel*>(QStringLiteral("plate"));
  assert(library_plate_ != nullptr);
  // Layers: folded at first, as the timeline rows show the same. It is
  // made before Objects so the layer list stays the second list among
  // the children.
  layers_fold_ = MakeOwned<FoldSection>(
      tr("Layers (top first)"), managers_.motion,
      managers_.document->SettingsPath(), QStringLiteral("cast_layers"),
      this, false);
  auto* layer_bar = MakeOwned<QWidget>(layers_fold_);
  layer_bar->setLayout(&layer_buttons_);
  layer_buttons_.setContentsMargins(0, 0, 0, 0);
  layers_fold_->form()->addRow(&layers_);
  layers_fold_->form()->addRow(layer_bar);
  // Objects: props from pictures, and the Background quick add.
  object_buttons_.addWidget(&add_objects_);
  object_buttons_.addWidget(&background_);
  object_buttons_.addStretch(1);
  auto* objects_body = MakeOwned<QWidget>(this);
  auto* objects_column = MakeOwned<QVBoxLayout>(objects_body);
  objects_column->setContentsMargins(0, 0, 0, 0);
  objects_column->addWidget(&objects_, 1);
  objects_column->addLayout(&object_buttons_);
  QWidget* objects = PlatedSection(tr("Objects"), objects_body, this);
  layout_.addWidget(library, 1);
  layout_.addWidget(objects, 1);
  layout_.addWidget(layers_fold_, 0);
  layout_.addStretch(0);
  objects_.SetEmptyText(
      tr("Drop PNG or GIF pictures here, or on the stage."));
  objects_.AcceptFiles();
  objects_.setToolTip(tr("Props made from pictures. Drag one onto the "
                         "stage; drop picture files here to add them."));
  add_objects_.setToolTip(tr("Make objects from PNG, GIF or WebP files."));
  // Without the object manager (some test benches) only Background shows.
  const bool has_objects = managers_.objects != nullptr;
  objects_.setVisible(has_objects);
  add_objects_.setVisible(has_objects);
  // Every list picks by the one rule; Delete asks the panel.
  connect(PickList::Install(&layers_), &PickList::DeleteAsked, this,
          &CastPanel::RemoveLayers);
  connect(&library_, &DollCards::DeleteAsked, this,
          [this] { DeleteInLibrary(); });
  connect(&objects_, &DollCards::DeleteAsked, this,
          [this] { TakeOut(objects_); });
  assert(layout_.count() > 0);
}

void CastPanel::Wire() {
  assert(managers_.IsComplete());
  assert(add_.menu() == &add_menu_);
  add_menu_.addAction(tr("Picture..."), this, &CastPanel::AddPicture);
  add_menu_.addAction(tr("Background..."), this, &CastPanel::AddBackground);
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
  connect(&remove_, &QPushButton::clicked, this, &CastPanel::RemoveLayers);
  connect(&background_, &QPushButton::clicked, this,
          &CastPanel::AddBackground);
  connect(&up_, &QPushButton::clicked, this, [this] { Restack(1); });
  connect(&down_, &QPushButton::clicked, this, [this] { Restack(-1); });
  connect(&library_, &QListWidget::itemSelectionChanged, this,
          &CastPanel::RefreshButtons);
  connect(&library_, &QListWidget::itemClicked, this,
          [this](const QListWidgetItem* item) { OpenClicked(item); });
  const bool has_folders = managers_.library_folders != nullptr;
  if (has_folders) {
    connect(nav_, &LibraryNavBar::Problem, this, &CastPanel::Report);
    connect(&library_, &QListWidget::itemActivated, this,
            [this](const QListWidgetItem* item) { OpenFolderCard(item); });
    connect(&library_, &DollCards::DollsDropped, this,
            &CastPanel::MoveDollsToFolder);
    connect(managers_.library_folders, &LibraryFolderManager::Changed, this,
            [this] { RefreshLibrary(); });
  }
  library_.setContextMenuPolicy(Qt::CustomContextMenu);
  connect(&library_, &QWidget::customContextMenuRequested, this,
          &CastPanel::ShowLibraryMenu);
  connect(&objects_, &DollCards::FilesDropped, this, &CastPanel::AddObjects);
  connect(&add_objects_, &QPushButton::clicked, this,
          &CastPanel::ChooseObjects);
  objects_.setContextMenuPolicy(Qt::CustomContextMenu);
  connect(&objects_, &QWidget::customContextMenuRequested, this,
          &CastPanel::ShowObjectMenu);
  connect(&objects_, &QListWidget::itemDoubleClicked, this,
          [this](const QListWidgetItem* item) {
            Report(PutDollsOnStage(managers_,
                                   {item->data(Qt::UserRole).toString()}));
          });
  connect(&layers_, &QListWidget::itemSelectionChanged, this,
          &CastPanel::PickLayer);
  connect(&layers_, &QListWidget::itemChanged, this, &CastPanel::ToggleShown);
  connect(managers_.library, &DollLibraryManager::LibraryChanged, this,
          [this] {
            pictures_.clear();
            RefreshLibrary();
          });
  connect(managers_.library, &DollLibraryManager::ArtChanged, this,
          [this](const QString& name) {
            stale_.insert(name);
            pictures_.remove(name);
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
