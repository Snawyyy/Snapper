#include "ui/cast_panel.h"

#include <QAbstractButton>
#include <QHBoxLayout>
#include <QSettings>

#include <cassert>
#include <initializer_list>
#include <variant>

#include "edit/character_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"
#include "edit/document_manager.h"
#include "ui/cassette_deck.h"
#include "ui/cel_stack.h"
#include "ui/divider_tabs.h"
#include "ui/character_drop.h"
#include "ui/panel_kit.h"
#include "ui/pick_list.h"
#include "ui/theme.h"
#include "ui/toast_manager.h"
#include "ui/toast_queue.h"

namespace snapper {
namespace {

constexpr char kTabKey[] = "cast_tab";

// Every tab's buttons are one row of equal keys across the page, so the
// row reads the same whichever tab is open.
void FillKeyRow(QHBoxLayout* row,
                std::initializer_list<QAbstractButton*> keys) {
  assert(row != nullptr);
  assert(keys.size() > 0);
  row->setSpacing(kTileGap);
  for (QAbstractButton* key : keys) {
    key->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row->addWidget(key, 1);
  }
}

}  // namespace

CastPanel::CastPanel(const Managers& managers)
    : managers_(managers),
      layout_(this),
      new_(tr("New")),
      edit_(tr("Edit")),
      place_(tr("Put on stage")),
      add_menu_(tr("Add")),
      copy_(tr("Copy")),
      remove_(tr("Delete")),
      up_(tr("Up")),
      down_(tr("Down")) {
  assert(managers_.IsComplete());
  BuildLayout();
  Wire();
  RefreshCharacters();
  RefreshLayers();
  assert(characters_.parent() != nullptr);
}

void CastPanel::BuildLayout() {
  assert(layout_.count() == 0);
  assert(tabs_ == nullptr);
  layout_.setContentsMargins(4, 4, 4, 4);
  layout_.setSpacing(theme::kRowGap);
  tabs_ = MakeOwned<DividerTabs>(this);
  tabs_->setDocumentMode(true);
  tabs_->addTab(MakeCharactersPage(), tr("Characters"));
  const int layers_tab = tabs_->addTab(MakeLayersPage(), tr("Layers"));
  tabs_->setTabToolTip(layers_tab, tr("The shot's layers, top first; "
                                       "the timeline rows show the same."));
  const QString settings_path = managers_.document->SettingsPath();
  const bool is_saved = !settings_path.isEmpty();
  if (is_saved) {
    const QSettings settings(settings_path, QSettings::IniFormat);
    tabs_->setCurrentIndex(settings.value(kTabKey, 0).toInt());
  }
  connect(tabs_, &QTabWidget::currentChanged, this,
          &CastPanel::RememberTab);
  layout_.addWidget(tabs_, 1);
  // The tape under the tabs turns with the playhead.
  layout_.addWidget(MakeOwned<CassetteDeck>(managers_), 0);
  characters_.setObjectName(QStringLiteral("characters"));
  layers_.setObjectName(QStringLiteral("layers"));
  characters_.setToolTip(tr("Drag a character onto the stage to put it "
                            "there; double click to edit it."));
  new_.setToolTip(tr("Make a new character and open it on the Character "
                     "tab."));
  // Every list picks by the one rule; Delete asks the panel.
  connect(PickList::Install(&layers_), &PickList::DeleteAsked, this,
          &CastPanel::RemoveLayers);
  // Layers are cels on a peg bar: drag to restack, eye and lock on each.
  CelStack::Install(&layers_);
  assert(layout_.count() > 0);
}

void CastPanel::RememberTab(int tab) const {
  assert(tab >= -1);
  assert(managers_.document != nullptr);
  const QString settings_path = managers_.document->SettingsPath();
  const bool is_saved = !settings_path.isEmpty() && tab >= 0;
  if (is_saved) {
    QSettings settings(settings_path, QSettings::IniFormat);
    settings.setValue(kTabKey, tab);
  }
}

QWidget* CastPanel::MakeCharactersPage() {
  assert(tabs_ != nullptr);
  assert(character_buttons_.count() == 0);
  FillKeyRow(&character_buttons_, {&new_, &edit_, &place_});
  auto* page = MakeOwned<QWidget>(this);
  auto* column = MakeOwned<QVBoxLayout>(page);
  column->setContentsMargins(DividerTabs::PageMargins());
  column->addWidget(&characters_, 1);
  column->addLayout(&character_buttons_);
  return page;
}

QWidget* CastPanel::MakeLayersPage() {
  assert(tabs_ != nullptr);
  assert(layer_buttons_.count() == 0);
  add_.setText(tr("Add"));
  add_.setMenu(&add_menu_);
  add_menu_.setToolTipsVisible(true);
  add_.setPopupMode(QToolButton::InstantPopup);
  FillKeyRow(&layer_buttons_, {&add_, &copy_, &remove_, &up_, &down_});
  auto* page = MakeOwned<QWidget>(this);
  auto* column = MakeOwned<QVBoxLayout>(page);
  column->setContentsMargins(DividerTabs::PageMargins());
  column->addWidget(&layers_, 1);
  column->addLayout(&layer_buttons_);
  return page;
}

void CastPanel::Wire() {
  assert(managers_.IsComplete());
  assert(add_.menu() == &add_menu_);
  add_menu_.addAction(tr("Picture..."), this, &CastPanel::AddPicture);
  add_menu_.addAction(tr("Background..."), this, &CastPanel::AddBackground);
  add_menu_.addAction(tr("Text..."), this, &CastPanel::AddText);
  add_menu_.addAction(tr("Storyboard"), this, [this] {
    const ShotId shot = managers_.selection->shot();
    Report(ProblemOf(managers_.stage->AddStoryboard(shot)));
  });
  QMenu* effects = add_menu_.addMenu(tr("Effect"));
  for (int kind = 0; kind < kEffectKindCount; ++kind) {
    const auto effect = static_cast<EffectKind>(kind);
    effects->addAction(EffectName(effect), this, [this, effect] {
      Report(ProblemOf(
          managers_.stage->AddEffect(managers_.selection->shot(), effect)));
    });
  }
  connect(&new_, &QPushButton::clicked, this, &CastPanel::NewCharacter);
  connect(&edit_, &QPushButton::clicked, this, [this] {
    const QStringList picked = PickedCharacters();
    if (!picked.isEmpty()) {
      OpenCharacter(picked.front());
    }
  });
  connect(&place_, &QPushButton::clicked, this, &CastPanel::Place);
  connect(&copy_, &QPushButton::clicked, this, [this] {
    Report(ProblemOf(managers_.stage->DuplicateAll(
        managers_.selection->shot(), managers_.selection->PickedLayers())));
  });
  connect(&remove_, &QPushButton::clicked, this, &CastPanel::RemoveLayers);
  connect(&up_, &QPushButton::clicked, this, [this] { Restack(1); });
  connect(&down_, &QPushButton::clicked, this, [this] { Restack(-1); });
  connect(&characters_, &QListWidget::itemSelectionChanged, this,
          &CastPanel::RefreshButtons);
  connect(&characters_, &QListWidget::itemActivated, this,
          [this](const QListWidgetItem* item) {
            OpenCharacter(item->data(Qt::UserRole).toString());
          });
  connect(&layers_, &QListWidget::itemSelectionChanged, this,
          &CastPanel::PickLayer);
  CelStack* cels = layers_.findChild<CelStack*>();
  assert(cels != nullptr);
  connect(cels, &CelStack::ShownClicked, this, &CastPanel::ToggleShown);
  connect(cels, &CelStack::LockClicked, this, &CastPanel::ToggleLocked);
  connect(cels, &CelStack::Moved, this, [this](LayerId layer, int index) {
    Report(ProblemOf(
        managers_.stage->Move(managers_.selection->shot(), layer, index)));
  });
  connect(managers_.history, &HistoryManager::Changed, this, [this] {
    RefreshCharacters();
    RefreshLayers();
  });
  connect(managers_.selection, &SelectionManager::Changed, this, [this] {
    RefreshCharacters();
    RefreshLayers();
  });
}

}  // namespace snapper
