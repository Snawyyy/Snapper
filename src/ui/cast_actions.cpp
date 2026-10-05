// CastPanel's lists and buttons.

#include <QFileDialog>
#include <QInputDialog>
#include <QSignalBlocker>

#include <algorithm>
#include <cassert>
#include <variant>

#include "edit/doll_library_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"
#include "ui/cast_panel.h"
#include "ui/form_helpers.h"

namespace snapper {
namespace {

// Each list item keeps its doll name or layer id here.
constexpr int kKeyRole = Qt::UserRole;

QString LayerKind(const Layer& layer) {
  assert(layer.id.IsValid());
  assert(layer.content.index() < 4);
  switch (layer.content.index()) {
    case 0:
      return QObject::tr("doll");
    case 1:
      return QObject::tr("picture");
    case 2:
      return QObject::tr("text");
    default:
      return QObject::tr("effect");
  }
}

// What the user should hear after an import or reload: the error, or
// what the rig lost because the art no longer has it.
QString Outcome(const QString& doll, const Result<ReconcileReport>& report) {
  assert(!doll.isEmpty());
  const bool is_failed = !report.has_value();
  if (is_failed) {
    return report.error().message;
  }
  const QStringList lost = report->missing + report->dropped_chains;
  assert(lost.size() < 100000);
  return lost.isEmpty() ? QString()
                        : QObject::tr("%1: gone from the art, so dropped "
                                      "from the rig: %2")
                              .arg(doll, lost.join(", "));
}

}  // namespace

QString CastPanel::PickedDoll() const {
  const QListWidgetItem* item = library_.currentItem();
  assert(library_.count() >= 0);
  assert(item == nullptr || item->data(kKeyRole).isValid());
  return item != nullptr ? item->data(kKeyRole).toString() : QString();
}

void CastPanel::RefreshLibrary() {
  assert(managers_.library != nullptr);
  const QString picked = PickedDoll();
  const QSignalBlocker quiet(library_);
  library_.clear();
  const Project& project = managers_.history->current();
  for (const QString& name : managers_.library->Available()) {
    const bool is_in = FindDoll(project, name) != nullptr;
    const bool is_stale = is_in && stale_.contains(name);
    const QString note = !is_in      ? QString()
                         : is_stale ? tr("  (art updated)")
                                    : tr("  (in project)");
    library_.addItem(name + note);
    QListWidgetItem* item = library_.item(library_.count() - 1);
    item->setData(kKeyRole, name);
    const bool is_picked = name == picked;
    if (is_picked) {
      library_.setCurrentItem(item);
    }
  }
  const QStringList& problems = managers_.library->conversion_problems();
  const bool has_problems = !problems.isEmpty();
  library_title_.setText(has_problems
                             ? tr("Doll library (%1 old doll(s) not "
                                  "converted)").arg(problems.size())
                             : tr("Doll library"));
  library_title_.setToolTip(problems.join("\n"));
  RefreshButtons();
}

void CastPanel::RefreshLayers() {
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
      item->setSelected(is_picked);
    }
  }
  RefreshButtons();
}

void CastPanel::RefreshButtons() {
  assert(managers_.library != nullptr);
  const QString doll = PickedDoll();
  const bool is_in =
      !doll.isEmpty() && FindDoll(managers_.history->current(), doll);
  const bool has_shot = managers_.selection->shot().IsValid();
  const bool has_layer = managers_.selection->layer().IsValid();
  Explain(&import_, managers_.library->WhyNoImport(doll));
  Explain(&reload_, is_in ? QString() : tr("Import the doll first."));
  Explain(&place_, !is_in      ? tr("Import the doll first.")
                   : has_shot ? QString()
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

void CastPanel::Import() {
  const QString doll = PickedDoll();
  assert(!doll.isEmpty());
  assert(managers_.library != nullptr);
  stale_.remove(doll);
  Report(Outcome(doll, managers_.library->Import(doll)));
}

void CastPanel::Reload() {
  const QString doll = PickedDoll();
  assert(!doll.isEmpty());
  assert(managers_.library != nullptr);
  stale_.remove(doll);
  Report(Outcome(doll, managers_.library->Reload(doll)));
  RefreshLibrary();
}

void CastPanel::Place() {
  const QString doll = PickedDoll();
  assert(!doll.isEmpty());
  assert(managers_.stage != nullptr);
  const auto layer = managers_.stage->AddDoll(managers_.selection->shot(),
                                              doll);
  if (layer) {
    managers_.selection->SelectLayer(*layer);
  }
  Report(ProblemOf(layer));
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
