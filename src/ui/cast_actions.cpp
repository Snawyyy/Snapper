// CastPanel's lists and buttons.

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QUrl>
#include <QInputDialog>
#include <QSignalBlocker>

#include <algorithm>
#include <cassert>
#include <variant>

#include "edit/character_manager.h"
#include "edit/doll_library_manager.h"
#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "edit/library_folder_manager.h"
#include "edit/object_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"
#include "io/character_file.h"
#include "ui/cast_panel.h"
#include "ui/character_thumb.h"
#include "ui/doll_drop.h"
#include "ui/form_helpers.h"
#include "ui/toast_manager.h"
#include "ui/toast_queue.h"

namespace snapper {
namespace {

// Each list item keeps its doll name or layer id here.
constexpr int kKeyRole = Qt::UserRole;

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
  assert(layer.content.index() < 6);
  switch (layer.content.index()) {
    case 0:
      return QObject::tr("doll");
    case 1:
      return QObject::tr("picture");
    case 2:
      return QObject::tr("text");
    case 4:
      return QObject::tr("storyboard");
    case 5:
      return QObject::tr("character");
    default:
      return QObject::tr("effect");
  }
}

// What the user should hear after an import or reload: the error, or
// what the rig lost because the art no longer has it.
QString Outcome(
    const Result<std::map<QString, ReconcileReport>>& reports) {
  assert(reports.has_value() || !reports.error().message.isEmpty());
  const bool is_failed = !reports.has_value();
  if (is_failed) {
    return reports.error().message;
  }
  QStringList lost;
  QStringList removed;
  QStringList remeshed;
  int carried = 0;
  int let_go = 0;
  for (const auto& [doll, report] : *reports) {
    for (const QString& gone : report.missing + report.dropped_chains) {
      lost.append(doll + ": " + gone);
    }
    for (const QString& drawing : report.removed_drawings) {
      removed.append(doll + " " + drawing);
    }
    remeshed += report.remeshed;
    carried += report.carried;
    let_go += report.lost;
  }
  assert(lost.size() < 100000);
  QStringList said;
  const bool has_lost = !lost.isEmpty();
  if (has_lost) {
    said.append(QObject::tr("Gone from the art, so dropped from the rig: %1")
                    .arg(lost.join(", ")));
  }
  const bool has_removed = !removed.isEmpty();
  if (has_removed) {
    said.append(QObject::tr("Drawings no longer in Krita: %1")
                    .arg(removed.join(", ")));
  }
  const bool has_remeshed = !remeshed.isEmpty();
  if (has_remeshed) {
    said.append(QObject::tr("Redrawn, so meshed again: %1 (%2 dots' "
                            "settings carried over, %3 let go)")
                    .arg(remeshed.join(", "))
                    .arg(carried)
                    .arg(let_go));
  }
  return said.join(". ");
}

// True when a layer of shot shows doll.
bool IsInShot(const Project& project, ShotId id, const QString& doll) {
  assert(id.value() >= 0);
  assert(!doll.isEmpty());
  const Shot* shot = FindShot(project, id);
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return false;
  }
  return std::any_of(shot->layers.begin(), shot->layers.end(),
                     [&doll](const Layer& layer) {
                       const auto* posed = std::get_if<DollLayer>(
                           &layer.content);
                       const auto* cast = std::get_if<CharacterLayer>(
                           &layer.content);
                       return (posed != nullptr && posed->doll == doll) ||
                              (cast != nullptr && cast->character == doll);
                     });
}

}  // namespace

QPixmap CastPanel::PictureOf(const QString& name) {
  assert(managers_.library != nullptr);
  assert(!name.isEmpty());
  // A project character is drawn fresh, so edits on the Character tab
  // show on its card.
  const Character* in_project =
      FindCharacter(managers_.history->current(), name);
  if (in_project != nullptr) {
    return CharacterPicture(*in_project, 72);
  }
  const auto found = pictures_.constFind(name);
  const bool is_drawn = found != pictures_.constEnd();
  if (is_drawn) {
    return *found;
  }
  const Character* character =
      FindCharacter(managers_.history->current(), name);
  const bool is_library_character =
      character == nullptr && managers_.characters != nullptr &&
      managers_.characters->Available().contains(name);
  const Result<Character> read =
      is_library_character
          ? ReadCharacterFolder(QDir(managers_.characters->folder()).filePath(name))
          : Result<Character>(std::unexpected(Error{}));
  const QPixmap picture =
      character != nullptr ? CharacterPicture(*character, 72)
      : read.has_value()   ? CharacterPicture(*read, 72)
                           : DollPicture(managers_.library->FolderOf(name), 72);
  pictures_.insert(name, picture);
  return picture;
}

void CastPanel::RefreshLibrary() {
  assert(managers_.IsComplete());
  assert(managers_.library != nullptr);
  const bool has_objects = managers_.objects != nullptr;
  QStringList all = has_objects ? managers_.objects->Dolls()
                                : managers_.library->Available();
  // Characters made on the Character tab sit beside the dolls.
  if (managers_.characters != nullptr) {
    for (const QString& name : managers_.characters->Available()) {
      if (!all.contains(name)) all.append(name);
    }
    for (const auto& [name, character] : managers_.history->current().characters) {
      if (!all.contains(name)) all.append(name);
    }
  }
  const LibraryFolderManager* folders = managers_.library_folders;
  const bool has_folders = folders != nullptr;
  FillCards(&library_,
            has_folders ? folders->DollsIn(folders->current(), all) : all);
  AddFolderCards();
  if (has_objects) {
    FillCards(&objects_, managers_.objects->Objects());
  }
  const QStringList& problems = managers_.library->conversion_problems();
  const bool has_problems = !problems.isEmpty();
  tabs_->setTabText(kLibraryTab,
                    has_problems ? tr("Library (%1 old doll(s) not "
                                      "converted)").arg(problems.size())
                                 : tr("Library"));
  tabs_->setTabToolTip(kLibraryTab, problems.join("\n"));
  RefreshButtons();
}

void CastPanel::FillCards(DollCards* cards, const QStringList& names) {
  assert(cards != nullptr);
  assert(names.size() < 100000);
  // Keep every picked doll picked across the rebuild.
  QSet<QString> picked;
  for (const QListWidgetItem* item : cards->selectedItems()) {
    picked.insert(item->data(kKeyRole).toString());
  }
  const QSignalBlocker quiet(cards);
  cards->clear();
  const Project& project = managers_.history->current();
  for (const QString& name : names) {
    const bool is_in = FindDoll(project, name) != nullptr ||
                       FindCharacter(project, name) != nullptr;
    const bool is_stale = is_in && stale_.contains(name);
    const bool is_in_shot =
        IsInShot(project, managers_.selection->shot(), name);
    const QString badge = is_stale     ? tr("New art")
                          : is_in_shot ? tr("In shot")
                          : is_in      ? tr("In project")
                                       : QString();
    cards->addItem(badge.isEmpty() ? name : name + "\n" + badge);
    QListWidgetItem* item = cards->item(cards->count() - 1);
    item->setData(kKeyRole, name);
    item->setIcon(PictureOf(name));
    item->setToolTip(is_stale ? tr("The art was re-exported from Krita: "
                                   "Reload takes it.")
                              : tr("Drag onto the stage to put it there."));
    if (picked.contains(name)) {
      KeepPicked(cards, cards->count() - 1);
    }
  }
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
      item->setData(kKeyRole, it->id.value());
      item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
      item->setCheckState(it->is_visible ? Qt::Checked : Qt::Unchecked);
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
  assert(managers_.library != nullptr);
  const QStringList out = PickedDolls(false);
  const bool is_in = !PickedDolls(true).isEmpty();
  const bool has_shot = managers_.selection->shot().IsValid();
  const bool has_layer = managers_.selection->layer().IsValid();
  const QStringList picked_dolls = PickedLibraryDolls();
  const QString picked_one =
      picked_dolls.isEmpty() ? QString() : picked_dolls.front();
  Explain(&import_, out.isEmpty()
                        ? managers_.library->WhyNoImport(picked_one)
                        : managers_.library->WhyNoImport(out.front()));
  Explain(&reload_, is_in ? QString() : tr("Import the doll first."));
  const bool has_doll = is_in || !out.isEmpty();
  Explain(&place_, !has_doll    ? tr("Pick a doll first.")
                   : has_shot ? QString()
                              : tr("Add a shot first."));
  Explain(&add_, has_shot ? QString() : tr("Add a shot first."));
  Explain(&background_,
          has_shot ? QString() : tr("Add a shot first."));
  if (has_shot) {
    background_.setToolTip(tr("Put a picture under everything, filling "
                              "the canvas."));
  }
  const QString no_layer = has_layer ? QString() : tr("Pick a layer first.");
  for (QWidget* button : {static_cast<QWidget*>(&copy_),
                          static_cast<QWidget*>(&remove_),
                          static_cast<QWidget*>(&up_),
                          static_cast<QWidget*>(&down_)}) {
    Explain(button, no_layer);
  }
}

QStringList CastPanel::PickedDolls(bool is_in_project) const {
  const Project& project = managers_.history->current();
  assert(library_.count() >= 0);
  QStringList dolls;
  for (const QListWidgetItem* item : library_.selectedItems()) {
    if (IsFolderCard(item)) {
      continue;
    }
    const QString name = item->data(kKeyRole).toString();
    const bool is_wanted =
        (FindDoll(project, name) != nullptr) == is_in_project;
    if (is_wanted) {
      dolls.append(name);
    }
  }
  assert(dolls.size() <= library_.count());
  return dolls;
}

void CastPanel::Import() {
  assert(managers_.IsComplete());
  const QStringList dolls = PickedDolls(false);
  assert(managers_.library != nullptr);
  Report(Outcome(managers_.library->ImportAll(dolls)));
  for (const QString& doll : dolls) {
    stale_.remove(doll);
  }
}

void CastPanel::OpenClicked(const QListWidgetItem* item) {
  assert(item != nullptr);
  assert(managers_.library != nullptr);
  const bool is_folder = IsFolderCard(item);
  if (is_folder) {
    return;
  }
  // Shift and Ctrl clicks are for picking many, not for opening one.
  const bool is_plain =
      QGuiApplication::keyboardModifiers() == Qt::NoModifier;
  if (!is_plain) {
    return;
  }
  const QString name = item->data(kKeyRole).toString();
  const bool is_in = FindDoll(managers_.history->current(), name) != nullptr;
  if (!is_in) {
    Report(Outcome(managers_.library->ImportAll({name})));
    stale_.remove(name);
  }
  const bool is_now_in =
      FindDoll(managers_.history->current(), name) != nullptr;
  if (is_now_in) {
    emit DollOpenAsked(name);
  }
}

void CastPanel::Reload() {
  assert(managers_.IsComplete());
  const QStringList dolls = PickedDolls(true);
  assert(managers_.library != nullptr);
  Report(Outcome(managers_.library->ReloadAll(dolls)));
  for (const QString& doll : dolls) {
    stale_.remove(doll);
  }
  RefreshLibrary();
}

void CastPanel::AddObjects(const QStringList& paths) {
  assert(managers_.objects != nullptr);
  assert(paths.size() < 100000);
  QStringList notes;
  const auto added = managers_.objects->AddToLibrary(paths, &notes);
  const bool is_failed = !added.has_value();
  if (is_failed) {
    Report(added.error().message);
    return;
  }
  pictures_.clear();
  RefreshLibrary();
  Report(notes.join(QStringLiteral(" ")));
}

void CastPanel::ChooseObjects() {
  assert(managers_.objects != nullptr);
  const QStringList paths = QFileDialog::getOpenFileNames(
      this, tr("Add objects"), QString(),
      tr("Pictures (*.png *.apng *.gif *.webp *.jpg *.jpeg *.bmp)"));
  const bool is_chosen = !paths.isEmpty();
  if (is_chosen) {
    AddObjects(paths);
  }
}

void CastPanel::ShowObjectMenu(const QPoint& at) {
  assert(managers_.library != nullptr);
  const QListWidgetItem* item = objects_.itemAt(at);
  if (item == nullptr) {
    return;
  }
  const QString name = item->data(kKeyRole).toString();
  assert(!name.isEmpty());
  QMenu menu;
  menu.setToolTipsVisible(true);
  menu.addAction(tr("Put on stage"), this, [this, name] {
    Report(PutDollsOnStage(managers_, {name}));
  });
  menu.addAction(tr("Show in folder"), this, [this, name] {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(managers_.library->FolderOf(name)));
  });
  QAction* remove = menu.addAction(tr("Take out of the project"), this,
                                   [this, name] {
                                     Report(ProblemOf(
                                         managers_.library->Remove(name)));
                                   });
  const bool is_in = FindDoll(managers_.history->current(), name) != nullptr;
  const int uses = is_in ? managers_.library->UseCount(name) : 0;
  remove->setEnabled(is_in && uses == 0);
  remove->setToolTip(!is_in      ? tr("%1 isn't in the project.").arg(name)
                     : uses > 0 ? tr("%1 is on %2 layer(s); delete those "
                                     "first.")
                                      .arg(name)
                                      .arg(uses)
                                : QString());
  menu.exec(objects_.viewport()->mapToGlobal(at));
}

void CastPanel::Place() {
  assert(managers_.IsComplete());
  assert(managers_.stage != nullptr);
  QStringList dolls = PickedDolls(true);
  dolls += PickedDolls(false);
  Report(PutDollsOnStage(managers_, dolls));
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

void CastPanel::TakeOut(const DollCards& cards) {
  assert(managers_.library != nullptr);
  assert(managers_.history != nullptr);
  QStringList names;
  for (const QListWidgetItem* item : cards.selectedItems()) {
    if (!IsFolderCard(item)) {
      names.append(item->data(kKeyRole).toString());
    }
  }
  const bool has_dolls = !names.isEmpty();
  if (!has_dolls) {
    return;
  }
  // The library says which can go; what it refuses it explains.
  QStringList problems;
  {
    const EditScope step(
        managers_.history,
        names.size() == 1 ? tr("Take %1 out of the project").arg(names[0])
                          : tr("Take %1 dolls out of the project")
                                .arg(names.size()));
    for (const QString& name : names) {
      const auto done = managers_.library->Remove(name);
      if (!done) {
        problems.append(done.error().message);
      }
    }
  }
  Report(problems.join(QStringLiteral(" ")));
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
    picks.insert({LayerId(item->data(kKeyRole).toInt()), QString()});
  }
  const QListWidgetItem* current = layers_.currentItem();
  const LayerId focus = current != nullptr && current->isSelected()
                            ? LayerId(current->data(kKeyRole).toInt())
                            : LayerId();
  managers_.selection->PickThings(picks, PickMode::kReplace, focus);
  assert(picks.size() <= static_cast<size_t>(kMaxLayersPerShot));
}

void CastPanel::ToggleShown(QListWidgetItem* item) {
  assert(item != nullptr);
  assert(managers_.stage != nullptr);
  const LayerId layer(item->data(kKeyRole).toInt());
  const bool is_shown = item->checkState() == Qt::Checked;
  // Ticking a picked layer ticks every picked layer.
  const auto picked = managers_.selection->PickedLayers();
  const bool is_in_pick =
      std::find(picked.begin(), picked.end(), layer) != picked.end();
  Report(ProblemOf(managers_.stage->ShowAll(
      managers_.selection->shot(),
      is_in_pick ? picked : std::vector<LayerId>{layer}, is_shown)));
}

}  // namespace snapper
