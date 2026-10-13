#include "ui/shot_bin.h"

#include <QPixmap>

#include <algorithm>
#include <cassert>
#include <memory>
#include <utility>

#include "edit/history_manager.h"
#include "edit/shot_manager.h"
#include "ui/pick_list.h"
#include "ui/problem.h"
#include "ui/toast_manager.h"

namespace snapper {
namespace {

constexpr int kThumbWidth = 96;
// Room for a picture and a shot's name beside it.
constexpr int kMinBinWidth = 200;
// The user role holding each item's shot id.
constexpr int kShotRole = Qt::UserRole;

}  // namespace

ShotId ShotOfDrop(const QMimeData* data) {
  assert(data != nullptr);
  assert(kShotMime[0] != '\0');
  const bool has_shot = data->hasFormat(kShotMime);
  if (!has_shot) {
    return ShotId();
  }
  bool is_number = false;
  const int id = QString::fromUtf8(data->data(kShotMime)).toInt(&is_number);
  return is_number ? ShotId(id) : ShotId();
}

ShotBin::ShotBin(const Managers& managers) : managers_(managers) {
  assert(managers_.IsComplete());
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setIconSize(QSize(kThumbWidth, kThumbWidth));
  setMinimumWidth(kMinBinWidth);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setToolTip(tr("Drag a shot onto a track below; double-click to open it "
                "on the Pose tab. Delete removes the picked shots."));
  // The one pick rule: Shift adds, Ctrl flips, Ctrl+A, Delete, Escape.
  PickList* picks = PickList::Install(this);
  connect(picks, &PickList::DeleteAsked, this, &ShotBin::RemovePicked);
  connect(this, &QListWidget::itemDoubleClicked, this,
          [this](const QListWidgetItem* item) {
            assert(item != nullptr);
            emit OpenShotAsked(ShotId(item->data(kShotRole).toInt()));
          });
  // While hidden (on another tab) edits only mark the list stale, so
  // posing never waits on pictures nobody sees.
  connect(managers_.history, &HistoryManager::Changed, this, [this] {
    is_stale_ = true;
    const bool is_shown = isVisible();
    if (is_shown) {
      Refresh();
    }
  });
  Refresh();
  assert(dragEnabled());
}

void ShotBin::showEvent(QShowEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  if (is_stale_) {
    Refresh();
  }
  QListWidget::showEvent(event);
}

void ShotBin::Refresh() {
  assert(managers_.history != nullptr);
  is_stale_ = false;
  const Project& project = managers_.history->current();
  const std::vector<ShotId> was = Picked();
  const int row = currentRow();
  clear();
  for (const auto& shot : project.shots) {
    // The list owns its items: add one, then dress it.
    addItem(tr("%1\n%2 s").arg(shot->name).arg(
        SecondsAtFrame(shot->length), 0, 'f', 1));
    QListWidgetItem* item = this->item(count() - 1);
    item->setIcon(Thumbnail(shot));
    item->setData(kShotRole, shot->id.value());
    // The pick outlives a refresh.
    item->setSelected(std::ranges::find(was, shot->id) != was.end());
  }
  setCurrentRow(std::min(row, count() - 1), QItemSelectionModel::NoUpdate);
  assert(count() == static_cast<int>(project.shots.size()));
}

std::vector<ShotId> ShotBin::Picked() const {
  assert(count() >= 0);
  std::vector<ShotId> shots;
  for (const QListWidgetItem* item : selectedItems()) {
    shots.emplace_back(item->data(kShotRole).toInt());
  }
  assert(shots.size() <= static_cast<size_t>(count()));
  return shots;
}

void ShotBin::RemovePicked() {
  assert(managers_.shots != nullptr);
  assert(managers_.toasts != nullptr);
  const std::vector<ShotId> shots = Picked();
  const bool has_pick = !shots.empty();
  if (!has_pick) {
    return;
  }
  managers_.toasts->Finish(ProblemOf(managers_.shots->RemoveAll(shots)),
                           shots.size() == 1 ? tr("Removed 1 shot")
                                             : tr("Removed %1 shots")
                                                   .arg(shots.size()));
}

QStringList ShotBin::mimeTypes() const {
  assert(kShotMime[0] != '\0');
  assert(dragEnabled());
  return {QString::fromLatin1(kShotMime)};
}

QMimeData* ShotBin::mimeData(const QList<QListWidgetItem*>& items) const {
  assert(items.size() >= 0);
  assert(dragEnabled());
  const bool is_empty = items.isEmpty();
  if (is_empty) {
    return nullptr;
  }
  // One shot drags: the one grabbed when it is among them.
  const QListWidgetItem* grabbed =
      items.contains(currentItem()) ? currentItem() : items.front();
  // The base makes the mime data the drag then owns; it adds the shot.
  QMimeData* dragged = QListWidget::mimeData(items);
  assert(dragged != nullptr);
  dragged->setData(kShotMime,
                   QByteArray::number(grabbed->data(kShotRole).toInt()));
  return dragged;
}

QIcon ShotBin::Thumbnail(const std::shared_ptr<const Shot>& shot) {
  assert(shot != nullptr);
  assert(managers_.history != nullptr);
  return QIcon(thumbs_.Of(managers_.history->current(), shot, kThumbWidth,
                          managers_.history->IsScopeOpen()));
}

}  // namespace snapper
