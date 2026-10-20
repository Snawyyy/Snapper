// CastPanel's lists and buttons.

#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QSignalBlocker>

#include <algorithm>
#include <cassert>
#include <variant>

#include "edit/character_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"
#include "io/character_file.h"
#include "ui/cast_panel.h"
#include "ui/cel_stack.h"
#include "ui/character_drop.h"
#include "ui/character_thumb.h"
#include "ui/form_helpers.h"
#include "ui/toast_manager.h"
#include "ui/toast_queue.h"

namespace snapper {
namespace {

// Each list item keeps its character name or layer id here.
constexpr int kKeyRole = Qt::UserRole;
constexpr int kCardPicture = 72;

// Marks row of list picked again after a rebuild. Through the selection
// model: under PickList the view is NoSelection, where an item's own
// setSelected does nothing.
void KeepPicked(QListWidget* list, int row) {
  assert(list != nullptr);
  assert(row >= 0 && row < list->count());
  list->selectionModel()->select(list->model()->index(row, 0),
                                 QItemSelectionModel::Select);
}

QString LayerKind(const Layer& layer) {
  assert(layer.id.IsValid());
  assert(layer.content.index() < std::variant_size_v<LayerContent>);
  switch (layer.content.index()) {
    case 0:
      return QObject::tr("character");
    case 1:
      return QObject::tr("picture");
    case 2:
      return QObject::tr("text");
    case 4:
      return QObject::tr("storyboard");
    default:
      return QObject::tr("effect");
  }
}

// layer of the picked shot, or null.
const Layer* PickedShotLayer(const Managers& managers, LayerId layer) {
  assert(managers.selection != nullptr);
  const Shot* shot =
      FindShot(managers.history->current(), managers.selection->shot());
  return shot != nullptr ? FindLayer(*shot, layer) : nullptr;
}

// True when a layer of shot shows character.
bool IsInShot(const Project& project, ShotId id, const QString& character) {
  assert(id.value() >= 0);
  assert(!character.isEmpty());
  const Shot* shot = FindShot(project, id);
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return false;
  }
  return std::any_of(shot->layers.begin(), shot->layers.end(),
                     [&character](const Layer& layer) {
                       const auto* cast =
                           std::get_if<CharacterLayer>(&layer.content);
                       return cast != nullptr && cast->character == character;
                     });
}

}  // namespace

QPixmap CastPanel::PictureOf(const QString& name) const {
  assert(managers_.characters != nullptr);
  assert(!name.isEmpty());
  // A project character is drawn fresh, so edits on the Character tab
  // show on its card.
  const Character* in_project =
      FindCharacter(managers_.history->current(), name);
  if (in_project != nullptr) {
    return CharacterPicture(*in_project, kCardPicture);
  }
  const Result<Character> read = ReadCharacterFolder(
      QDir(managers_.characters->folder()).filePath(name));
  return read.has_value() ? CharacterPicture(*read, kCardPicture)
                          : QPixmap();
}

void CastPanel::RefreshCharacters() {
  assert(managers_.IsComplete());
  assert(managers_.characters != nullptr);
  QStringList names = managers_.characters->Available();
  const Project& project = managers_.history->current();
  for (const auto& [name, character] : project.characters) {
    if (!names.contains(name)) {
      names.append(name);
    }
  }
  names.sort(Qt::CaseInsensitive);
  // Keep every picked character picked across the rebuild.
  const QStringList picked = PickedCharacters();
  const QSignalBlocker quiet(characters_);
  characters_.clear();
  for (const QString& name : names) {
    const bool is_in = FindCharacter(project, name) != nullptr;
    const bool is_in_shot =
        IsInShot(project, managers_.selection->shot(), name);
    const QString badge = is_in_shot ? tr("In shot")
                          : is_in    ? tr("In project")
                                     : QString();
    characters_.addItem(badge.isEmpty() ? name : name + "\n" + badge);
    QListWidgetItem* item = characters_.item(characters_.count() - 1);
    item->setData(kKeyRole, name);
    item->setIcon(PictureOf(name));
    if (picked.contains(name)) {
      KeepPicked(&characters_, characters_.count() - 1);
    }
  }
  RefreshButtons();
}

void CastPanel::RefreshLayers() {
  assert(managers_.IsComplete());
  assert(managers_.selection != nullptr);
  const QSignalBlocker quiet(layers_);
  layers_.clear();
  const Shot* shot =
      FindShot(managers_.history->current(), managers_.selection->shot());
  const bool has_shot = shot != nullptr;
  if (has_shot) {
    for (auto it = shot->layers.rbegin(); it != shot->layers.rend(); ++it) {
      layers_.addItem(tr("%1 (%2)").arg(it->name, LayerKind(*it)));
      QListWidgetItem* item = layers_.item(layers_.count() - 1);
      item->setData(kCelIdRole, it->id.value());
      item->setData(kCelShownRole, it->is_visible);
      item->setData(kCelLockedRole, it->is_locked);
      item->setData(kCelKindRole, static_cast<int>(it->content.index()));
      const auto picked = managers_.selection->PickedLayers();
      const bool is_picked =
          std::find(picked.begin(), picked.end(), it->id) != picked.end();
      if (is_picked) {
        KeepPicked(&layers_, layers_.count() - 1);
      }
    }
  }
  RefreshButtons();
}

void CastPanel::RefreshButtons() {
  assert(managers_.IsComplete());
  assert(managers_.selection != nullptr);
  const bool has_character = !PickedCharacters().isEmpty();
  const bool has_shot = managers_.selection->shot().IsValid();
  const bool has_layer = managers_.selection->layer().IsValid();
  Explain(&edit_, has_character ? QString() : tr("Pick a character first."));
  Explain(&place_, !has_character ? tr("Pick a character first.")
                   : has_shot     ? QString()
                                  : tr("Add a shot first."));
  Explain(&add_, has_shot ? QString() : tr("Add a shot first."));
  const QString no_layer = has_layer ? QString() : tr("Pick a layer first.");
  for (QWidget* button : {static_cast<QWidget*>(&copy_),
                          static_cast<QWidget*>(&remove_),
                          static_cast<QWidget*>(&up_),
                          static_cast<QWidget*>(&down_)}) {
    Explain(button, no_layer);
  }
}

QStringList CastPanel::PickedCharacters() const {
  assert(characters_.count() >= 0);
  QStringList names;
  for (const QListWidgetItem* item : characters_.selectedItems()) {
    names.append(item->data(kKeyRole).toString());
  }
  assert(names.size() <= characters_.count());
  return names;
}

void CastPanel::NewCharacter() {
  assert(managers_.characters != nullptr);
  bool is_ok = false;
  const QString name = QInputDialog::getText(
      this, tr("New character"), tr("Name"), QLineEdit::Normal,
      tr("Character"), &is_ok);
  if (!is_ok) {
    return;
  }
  const auto made = managers_.characters->Create(name);
  if (!made) {
    Report(made.error().message);
    return;
  }
  emit CharacterOpenAsked(*made);
}

void CastPanel::OpenCharacter(const QString& name) {
  assert(managers_.characters != nullptr);
  assert(!name.isEmpty());
  const bool is_in =
      FindCharacter(managers_.history->current(), name) != nullptr;
  if (!is_in) {
    const auto imported = managers_.characters->Import(name);
    if (!imported) {
      Report(imported.error().message);
      return;
    }
  }
  emit CharacterOpenAsked(name);
}

void CastPanel::Place() {
  assert(managers_.IsComplete());
  assert(managers_.stage != nullptr);
  Report(PutCharactersOnStage(managers_, PickedCharacters()));
}

void CastPanel::AddPicture() {
  assert(managers_.stage != nullptr);
  assert(managers_.selection != nullptr);
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Add picture"), QString(),
      tr("Pictures (*.png *.jpg *.jpeg *.webp *.bmp)"));
  const bool is_chosen = !path.isEmpty();
  if (is_chosen) {
    Report(ProblemOf(
        managers_.stage->AddImage(managers_.selection->shot(), path)));
  }
}

void CastPanel::AddBackground() {
  assert(managers_.stage != nullptr);
  assert(managers_.selection != nullptr);
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Add background"), QString(),
      tr("Pictures (*.png *.jpg *.jpeg *.webp *.bmp)"));
  const bool is_chosen = !path.isEmpty();
  if (is_chosen) {
    Report(ProblemOf(
        managers_.stage->AddBackground(managers_.selection->shot(), path)));
  }
}

void CastPanel::RemoveLayers() {
  assert(managers_.stage != nullptr);
  assert(managers_.toasts != nullptr);
  const auto picked = managers_.selection->PickedLayers();
  const bool has_picks = !picked.empty();
  if (!has_picks) {
    return;
  }
  managers_.toasts->Finish(
      ProblemOf(managers_.stage->RemoveAll(managers_.selection->shot(),
                                           picked)),
      CountText(tr("Deleted"), static_cast<int>(picked.size()), tr("layer"),
                tr("layers")));
}

void CastPanel::AddText() {
  assert(managers_.stage != nullptr);
  assert(managers_.selection != nullptr);
  bool is_ok = false;
  const QString text = QInputDialog::getMultiLineText(
      this, tr("Add text"), tr("Words (Enter for a line break)"), QString(),
      &is_ok);
  if (is_ok) {
    Report(ProblemOf(
        managers_.stage->AddText(managers_.selection->shot(), text)));
  }
}

void CastPanel::Restack(int step) {
  assert(step == 1 || step == -1);
  assert(managers_.stage != nullptr);
  Report(ProblemOf(managers_.stage->Restack(
      managers_.selection->shot(), managers_.selection->PickedLayers(),
      step)));
}

void CastPanel::PickLayer() {
  assert(managers_.selection != nullptr);
  std::set<Pick> picks;
  for (const QListWidgetItem* item : layers_.selectedItems()) {
    picks.insert(Pick{LayerId(item->data(kKeyRole).toInt())});
  }
  const QListWidgetItem* current = layers_.currentItem();
  const LayerId focus = current != nullptr && current->isSelected()
                            ? LayerId(current->data(kKeyRole).toInt())
                            : LayerId();
  managers_.selection->PickThings(picks, PickMode::kReplace, focus);
  assert(picks.size() <= static_cast<size_t>(kMaxLayersPerShot));
}

void CastPanel::ToggleShown(LayerId layer) {
  assert(layer.IsValid());
  assert(managers_.stage != nullptr);
  const Layer* found = PickedShotLayer(managers_, layer);
  if (found == nullptr) {
    return;
  }
  const bool is_shown = !found->is_visible;
  // Ticking a picked layer ticks every picked layer.
  const auto picked = managers_.selection->PickedLayers();
  const bool is_in_pick =
      std::find(picked.begin(), picked.end(), layer) != picked.end();
  Report(ProblemOf(managers_.stage->ShowAll(
      managers_.selection->shot(),
      is_in_pick ? picked : std::vector<LayerId>{layer}, is_shown)));
}

void CastPanel::ToggleLocked(LayerId layer) {
  assert(layer.IsValid());
  assert(managers_.stage != nullptr);
  const Layer* found = PickedShotLayer(managers_, layer);
  if (found != nullptr) {
    Report(ProblemOf(managers_.stage->SetLocked(
        managers_.selection->shot(), layer, !found->is_locked)));
  }
}

}  // namespace snapper
