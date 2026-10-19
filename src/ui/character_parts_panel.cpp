#include "ui/character_parts_panel.h"

#include <QDropEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QVBoxLayout>

#include <algorithm>
#include <cassert>

#include "edit/character_manager.h"
#include "edit/history_manager.h"
#include "ui/panel_kit.h"
#include "ui/part_icons.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kIdRole = Qt::UserRole;
// The picker's data for a library character not yet in the project.
constexpr int kLibraryRole = Qt::UserRole + 1;

PartId IdOf(const QTreeWidgetItem* item) {
  assert(item != nullptr);
  assert(item->columnCount() >= 1);
  return PartId(item->data(0, kIdRole).toInt());
}

}  // namespace

PartTree::PartTree(QWidget* parent) : QTreeWidget(parent) {
  setColumnCount(2);
  setHeaderHidden(true);
  setDragDropMode(QAbstractItemView::InternalMove);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setEditTriggers(QAbstractItemView::DoubleClicked |
                  QAbstractItemView::EditKeyPressed);
  setIndentation(14);
  setIconSize(QSize(18, 18));
  header()->setStretchLastSection(false);
  header()->setSectionResizeMode(0, QHeaderView::Stretch);
  header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  setRootIsDecorated(true);
}

void PartTree::dropEvent(QDropEvent* event) {
  assert(event != nullptr);
  assert(columnCount() == 2);
  QTreeWidgetItem* moved = currentItem();
  if (moved == nullptr) {
    event->ignore();
    return;
  }
  const PartId part = IdOf(moved);
  QTreeWidgetItem* target = itemAt(event->position().toPoint());
  const DropIndicatorPosition where = dropIndicatorPosition();
  PartId parent;
  int row = 0;
  if (target == nullptr || where == QAbstractItemView::OnViewport) {
    row = topLevelItemCount();
  } else if (where == QAbstractItemView::OnItem) {
    parent = IdOf(target);
    row = 0;
  } else {
    QTreeWidgetItem* holder = target->parent();
    parent = holder != nullptr ? IdOf(holder) : PartId();
    const int index = holder != nullptr ? holder->indexOfChild(target)
                                        : indexOfTopLevelItem(target);
    row = where == QAbstractItemView::BelowItem ? index + 1 : index;
  }
  // The tree is rebuilt from the character after the move.
  event->setDropAction(Qt::IgnoreAction);
  event->accept();
  emit Dropped(part, parent, row);
}

CharacterPartsPanel::CharacterPartsPanel(HistoryManager* history,
                                         CharacterManager* characters,
                                         CharacterSession* session,
                                         QWidget* parent)
    : QWidget(parent),
      history_(history),
      characters_(characters),
      session_(session) {
  assert(history_ != nullptr && characters_ != nullptr);
  assert(session_ != nullptr);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(theme::kIndent / 2, theme::kIndent / 2,
                             theme::kIndent / 2, theme::kIndent / 2);
  layout->setSpacing(theme::kRowGap);
  auto* top = new QHBoxLayout;
  top->setSpacing(theme::kRowGap / 2);
  top->addWidget(&picker_, 1);
  new_button_.setText(tr("New"));
  new_button_.setToolTip(tr("Make a new character with bones and a drawing"));
  save_button_.setText(tr("Save"));
  save_button_.setToolTip(
      tr("Write this character to the library, for other projects"));
  top->addWidget(&new_button_);
  top->addWidget(&save_button_);
  layout->addLayout(top);
  layout->addWidget(&tree_, 1);
  struct Tile {
    QPushButton* button;
    PartKind kind;
    const char* tip;
  };
  const Tile tiles[] = {
      {&add_drawing_, PartKind::kVector, "Add a drawing part: points and shapes"},
      {&add_bones_, PartKind::kBone, "Add a bone part: bones bend what is inside"},
      {&add_group_, PartKind::kGroup, "Add a group to move parts as one"},
      {&add_switch_, PartKind::kSwitch,
       "Add a switch: shows one of its parts at a time (mouths, hands)"},
      {&add_picture_, PartKind::kImage, "Add a picture from a file"},
  };
  std::vector<QWidget*> row;
  for (const Tile& tile : tiles) {
    tile.button->setIcon(PartIcon(tile.kind));
    tile.button->setIconSize(QSize(20, 20));
    tile.button->setToolTip(tr(tile.tip));
    row.push_back(tile.button);
  }
  duplicate_.setText(tr("Copy"));
  duplicate_.setToolTip(tr("Duplicate the picked part with what it holds"));
  remove_.setText(tr("Delete"));
  remove_.setToolTip(tr("Delete the picked part with what it holds"));
  layout->addWidget(TileRow(row, this));
  layout->addWidget(TileRow({&duplicate_, &remove_}, this));
  for (const Tile& tile : tiles) {
    if (tile.kind == PartKind::kImage) {
      connect(tile.button, &QPushButton::clicked, this,
              &CharacterPartsPanel::AddPicture);
    } else {
      const PartKind kind = tile.kind;
      connect(tile.button, &QPushButton::clicked, this,
              [this, kind] { AddPart(kind); });
    }
  }
  connect(&new_button_, &QPushButton::clicked, this, [this] {
    bool is_ok = false;
    const QString name = QInputDialog::getText(
        this, tr("New character"), tr("Name:"), QLineEdit::Normal,
        tr("Character"), &is_ok);
    if (!is_ok) {
      return;
    }
    const auto made = characters_->Create(name);
    if (!made) {
      emit Problem(made.error().message);
      return;
    }
    session_->SetCharacter(*made);
  });
  connect(&save_button_, &QPushButton::clicked, this, [this] {
    Report(characters_->SaveToLibrary(session_->character()));
  });
  connect(&picker_, &QComboBox::activated, this, [this](int index) {
    const QString library = picker_.itemData(index, kLibraryRole).toString();
    if (!library.isEmpty()) {
      Report(characters_->Import(library));
    }
    session_->SetCharacter(picker_.itemData(index).toString());
  });
  connect(&tree_, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem* item) {
            if (!is_filling_ && item != nullptr) {
              session_->SetPart(IdOf(item));
            }
          });
  connect(&tree_, &QTreeWidget::itemChanged, this,
          [this](QTreeWidgetItem* item, int column) {
            if (is_filling_) {
              return;
            }
            if (column == 0) {
              Report(characters_->RenamePart(session_->character(), IdOf(item),
                                             item->text(0)));
            } else {
              Report(characters_->SetVisible(
                  session_->character(), IdOf(item),
                  item->checkState(1) == Qt::Checked));
            }
          });
  connect(&tree_, &PartTree::Dropped, this,
          [this](PartId part, PartId holder, int at) {
            const Character* character =
                FindCharacter(history_->current(), session_->character());
            if (character == nullptr) {
              return;
            }
            // row counts from the front with part still in place.
            std::vector<PartId> front = ChildrenOf(*character, holder);
            std::ranges::reverse(front);
            const auto was = std::ranges::find(front, part);
            int to = at;
            if (was != front.end()) {
              if (was - front.begin() < at) {
                --to;
              }
              front.erase(was);
            }
            const int count = static_cast<int>(front.size());
            Report(characters_->MovePart(session_->character(), part, holder,
                                         count - std::clamp(to, 0, count)));
          });
  connect(&duplicate_, &QPushButton::clicked, this, [this] {
    const auto copy =
        characters_->DuplicatePart(session_->character(), session_->part());
    if (!copy) {
      emit Problem(copy.error().message);
      return;
    }
    session_->SetPart(*copy);
  });
  connect(&remove_, &QPushButton::clicked, this, [this] {
    const std::vector<PartId> ids = {session_->part()};
    Report(characters_->RemoveParts(session_->character(), ids));
    session_->SetPart(PartId());
  });
  connect(history_, &HistoryManager::Changed, this,
          &CharacterPartsPanel::Refresh);
  connect(session_, &CharacterSession::Changed, this,
          &CharacterPartsPanel::Refresh);
  Refresh();
}

void CharacterPartsPanel::Report(const Result<void>& done) {
  assert(history_ != nullptr);
  assert(session_ != nullptr);
  emit Problem(done ? QString() : done.error().message);
}

void CharacterPartsPanel::AddPart(PartKind kind) {
  assert(static_cast<int>(kind) < kPartKindCount);
  assert(session_ != nullptr);
  const Character* character =
      FindCharacter(history_->current(), session_->character());
  if (character == nullptr) {
    emit Problem(tr("Make or pick a character first."));
    return;
  }
  // A new part goes into the picked folder, or beside the picked part.
  const CharacterPart* picked = FindPart(*character, session_->part());
  const PartId parent = picked == nullptr       ? PartId()
                        : IsFolder(picked->kind) ? picked->id
                                                 : picked->parent;
  const auto added = characters_->AddPart(session_->character(), kind, parent);
  if (!added) {
    emit Problem(added.error().message);
    return;
  }
  session_->SetPart(*added);
}

void CharacterPartsPanel::AddPicture() {
  assert(session_ != nullptr);
  assert(characters_ != nullptr);
  const Character* character =
      FindCharacter(history_->current(), session_->character());
  if (character == nullptr) {
    emit Problem(tr("Make or pick a character first."));
    return;
  }
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Add a picture"), QString(),
      tr("Pictures (*.png *.jpg *.jpeg *.webp *.bmp)"));
  if (path.isEmpty()) {
    return;
  }
  const CharacterPart* picked = FindPart(*character, session_->part());
  const PartId parent = picked == nullptr       ? PartId()
                        : IsFolder(picked->kind) ? picked->id
                                                 : picked->parent;
  const auto added =
      characters_->AddImagePart(session_->character(), parent, path);
  if (!added) {
    emit Problem(added.error().message);
    return;
  }
  session_->SetPart(*added);
}

void CharacterPartsPanel::RefreshCharacters() {
  assert(history_ != nullptr);
  assert(characters_ != nullptr);
  picker_.clear();
  const Project& project = history_->current();
  for (const auto& [name, character] : project.characters) {
    picker_.addItem(name, name);
  }
  bool has_separator = false;
  for (const QString& name : characters_->Available()) {
    if (project.characters.contains(name)) {
      continue;
    }
    if (!has_separator) {
      picker_.insertSeparator(picker_.count());
      has_separator = true;
    }
    picker_.addItem(tr("Add %1 from the library").arg(name), name);
    picker_.setItemData(picker_.count() - 1, name, kLibraryRole);
  }
  const int at = picker_.findData(session_->character());
  picker_.setCurrentIndex(at);
  if (at < 0 && !project.characters.empty()) {
    picker_.setCurrentIndex(0);
    session_->SetCharacter(project.characters.begin()->first);
  }
}

void CharacterPartsPanel::FillTree(const Character& character, PartId parent,
                                   QTreeWidgetItem* under) {
  assert(character.parts.size() <= static_cast<size_t>(kMaxCharacterParts));
  assert(parent.value() >= 0);
  const std::vector<PartId> children = ChildrenOf(character, parent);
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    const CharacterPart* part = FindPart(character, *it);
    auto* item = under != nullptr ? new QTreeWidgetItem(under)
                                  : new QTreeWidgetItem(&tree_);
    item->setText(0, part->name);
    item->setIcon(0, PartIcon(part->kind));
    item->setToolTip(0, PartKindName(part->kind));
    item->setData(0, kIdRole, part->id.value());
    item->setCheckState(1, part->is_visible ? Qt::Checked : Qt::Unchecked);
    item->setToolTip(1, tr("Shown"));
    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                          Qt::ItemIsEditable | Qt::ItemIsDragEnabled |
                          Qt::ItemIsUserCheckable;
    if (IsFolder(part->kind)) {
      flags |= Qt::ItemIsDropEnabled;
    }
    item->setFlags(flags);
    if (part->id == session_->part()) {
      tree_.setCurrentItem(item);
    }
    FillTree(character, part->id, item);
    item->setExpanded(true);
  }
}

void CharacterPartsPanel::Refresh() {
  assert(history_ != nullptr);
  assert(session_ != nullptr);
  is_filling_ = true;
  RefreshCharacters();
  tree_.clear();
  const Character* character =
      session_->character().isEmpty()
          ? nullptr
          : FindCharacter(history_->current(), session_->character());
  if (character != nullptr) {
    FillTree(*character, PartId(), nullptr);
  }
  const bool has_character = character != nullptr;
  const bool has_part = has_character && session_->part().IsValid() &&
                        FindPart(*character, session_->part()) != nullptr;
  for (QPushButton* button :
       {&add_drawing_, &add_bones_, &add_group_, &add_switch_, &add_picture_,
        &save_button_}) {
    button->setEnabled(has_character);
  }
  duplicate_.setEnabled(has_part);
  remove_.setEnabled(has_part);
  const QString why = tr("Pick a part first");
  duplicate_.setToolTip(has_part ? tr("Duplicate the picked part") : why);
  remove_.setToolTip(has_part ? tr("Delete the picked part") : why);
  is_filling_ = false;
}

}  // namespace snapper
