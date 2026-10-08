#include "ui/shot_bin.h"

#include <QPixmap>

#include <algorithm>
#include <cassert>
#include <utility>

#include "edit/history_manager.h"

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
  setSelectionMode(QAbstractItemView::SingleSelection);
  setIconSize(QSize(kThumbWidth, kThumbWidth));
  setMinimumWidth(kMinBinWidth);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setToolTip(tr("Drag a shot onto a track below."));
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
  const int row = currentRow();
  clear();
  std::map<std::shared_ptr<const Shot>, QIcon> kept;
  for (const auto& shot : project.shots) {
    auto* item = new QListWidgetItem(
        Thumbnail(shot),
        tr("%1\n%2 s").arg(shot->name).arg(
            SecondsAtFrame(shot->length), 0, 'f', 1),
        this);
    item->setData(kShotRole, shot->id.value());
    kept[shot] = thumbnails_[shot];
  }
  // Pictures of shots that were edited or removed go.
  thumbnails_ = std::move(kept);
  setCurrentRow(std::min(row, count() - 1));
  assert(count() == static_cast<int>(project.shots.size()));
}

QStringList ShotBin::mimeTypes() const {
  assert(kShotMime[0] != '\0');
  assert(dragEnabled());
  return {QString::fromLatin1(kShotMime)};
}

QMimeData* ShotBin::mimeData(const QList<QListWidgetItem*>& items) const {
  assert(items.size() >= 0);
  assert(dragEnabled());
  const bool is_one = items.size() == 1;
  if (!is_one) {
    return nullptr;
  }
  auto* dragged = new QMimeData();
  dragged->setData(kShotMime,
                   QByteArray::number(items.front()->data(kShotRole).toInt()));
  return dragged;
}

QIcon ShotBin::Thumbnail(const std::shared_ptr<const Shot>& shot) {
  assert(shot != nullptr);
  assert(managers_.history != nullptr);
  const auto found = thumbnails_.find(shot);
  const bool is_drawn = found != thumbnails_.end() && !found->second.isNull();
  if (is_drawn) {
    return found->second;
  }
  const Project& project = managers_.history->current();
  const double scale = static_cast<double>(kThumbWidth) /
                       std::max(project.canvas.width, project.canvas.height);
  const QIcon icon(QPixmap::fromImage(
      renderer_.RenderShot(project, *shot, Frame(0), scale)));
  thumbnails_[shot] = icon;
  return icon;
}

}  // namespace snapper
