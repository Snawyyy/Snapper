#include "edit/palette_manager.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>

#include <algorithm>
#include <cassert>
#include <variant>

#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"

namespace snapper {
namespace {

// The cache keeps this many drawings before it starts over.
constexpr size_t kMaxCachedDrawings = 4096;

std::vector<QColor> ToColors(const std::vector<ColorCount>& counts) {
  std::vector<QColor> colors;
  colors.reserve(counts.size());
  for (const ColorCount& count : counts) {
    colors.push_back(QColor::fromRgba(count.rgb));
  }
  return colors;
}

}  // namespace

PaletteManager::PaletteManager(HistoryManager* history,
                               SelectionManager* selection)
    : history_(history), selection_(selection) {
  assert(history_ != nullptr);
  assert(selection_ != nullptr);
  connect(history_, &HistoryManager::Changed, this, &PaletteManager::Changed);
  connect(selection_, &SelectionManager::Changed, this,
          &PaletteManager::Changed);
}

const Doll* PaletteManager::PickedDoll() const {
  const Project& project = history_->current();
  const Shot* shot = FindShot(project, selection_->shot());
  // Nothing picked yet (a project just opened): no layer to look up.
  const bool has_layer = selection_->layer().IsValid();
  const bool is_pickable = shot != nullptr && has_layer;
  if (!is_pickable) {
    return nullptr;
  }
  const Layer* layer = FindLayer(*shot, selection_->layer());
  if (layer == nullptr) {
    return nullptr;
  }
  const auto* posed = std::get_if<DollLayer>(&layer->content);
  return posed != nullptr ? FindDoll(project, posed->doll) : nullptr;
}

QString PaletteManager::DollName() const {
  const Doll* doll = PickedDoll();
  return doll != nullptr ? doll->name : QString();
}

QString PaletteManager::PickedPiece() const {
  const LayerId focus = selection_->layer();
  for (const snapper::Pick& pick : selection_->picks()) {
    const bool is_piece = pick.layer == focus && !pick.piece.isEmpty();
    if (is_piece) {
      return pick.piece;
    }
  }
  return {};
}

const std::vector<ColorCount>& PaletteManager::ColorsOf(
    const QString& path) const {
  assert(!path.isEmpty());
  const QDateTime stamp = QFileInfo(path).lastModified();
  const auto found = cache_.find(path);
  const bool is_fresh = found != cache_.end() && found->second.stamp == stamp;
  if (is_fresh) {
    return found->second.colors;
  }
  if (cache_.size() >= kMaxCachedDrawings) {
    cache_.clear();
  }
  Cached& entry = cache_[path];
  entry.stamp = stamp;
  entry.colors = SolidColors(QImage(path));
  return entry.colors;
}

std::vector<QColor> PaletteManager::PieceColors() const {
  const Doll* doll = PickedDoll();
  const QString piece = PickedPiece();
  if (doll == nullptr || piece.isEmpty()) {
    return {};
  }
  const ArtPiece* art = FindArt(*doll, piece);
  if (art == nullptr) {
    return {};
  }
  const QDir folder(doll->folder);
  std::vector<std::vector<ColorCount>> lists;
  for (const QString& drawing : art->drawings) {
    lists.push_back(ColorsOf(folder.filePath(drawing)));
  }
  return ToColors(MergeColors(lists));
}

std::vector<QColor> PaletteManager::DollColors() const {
  const Doll* doll = PickedDoll();
  if (doll == nullptr) {
    return {};
  }
  const QDir folder(doll->folder);
  std::vector<std::vector<ColorCount>> lists;
  for (const ArtPiece& art : doll->art.pieces) {
    for (const QString& drawing : art.drawings) {
      lists.push_back(ColorsOf(folder.filePath(drawing)));
    }
  }
  return ToColors(MergeColors(lists));
}

std::vector<QColor> PaletteManager::RecentColors() const {
  return history_->current().recent_colors;
}

std::vector<QColor> PaletteManager::AddedColors() const {
  const Doll* doll = PickedDoll();
  return doll != nullptr ? doll->rig.added_colors : std::vector<QColor>();
}

QString PaletteManager::WhyNoAdd() const {
  const Doll* doll = PickedDoll();
  if (doll == nullptr) {
    return Tr("Pick a doll first; added colors are kept with it.");
  }
  const bool is_full =
      doll->rig.added_colors.size() >= static_cast<size_t>(kMaxAddedColors);
  if (is_full) {
    return Tr("A doll keeps at most %1 added colors.").arg(kMaxAddedColors);
  }
  return {};
}

Result<void> PaletteManager::AddColor(const QColor& color) {
  const QString why = WhyNoAdd();
  if (!why.isEmpty()) {
    return std::unexpected(Error{why});
  }
  if (!color.isValid()) {
    return std::unexpected(Error{Tr("That isn't a color.")});
  }
  std::vector<QColor> colors = AddedColors();
  const bool is_there =
      std::find(colors.begin(), colors.end(), color) != colors.end();
  if (is_there) {
    return {};
  }
  colors.push_back(color);
  return WriteAdded(Tr("Add color %1").arg(color.name()), std::move(colors));
}

Result<void> PaletteManager::RemoveColor(const QColor& color) {
  std::vector<QColor> colors = AddedColors();
  const auto found = std::find(colors.begin(), colors.end(), color);
  if (found == colors.end()) {
    return std::unexpected(
        Error{Tr("%1 isn't an added color.").arg(color.name())});
  }
  colors.erase(found);
  return WriteAdded(Tr("Remove color %1").arg(color.name()),
                    std::move(colors));
}

Result<void> PaletteManager::WriteAdded(const QString& label,
                                        std::vector<QColor> colors) {
  assert(colors.size() <= static_cast<size_t>(kMaxAddedColors));
  const Doll* doll = PickedDoll();
  if (doll == nullptr) {
    return std::unexpected(Error{Tr("Pick a doll first.")});
  }
  Doll changed = *doll;
  changed.rig.added_colors = std::move(colors);
  Project next = history_->current();
  next.dolls[changed.name] = std::make_shared<const Doll>(std::move(changed));
  return history_->Apply(label, std::move(next));
}

void PaletteManager::Pick(const QColor& color,
                          Qt::KeyboardModifiers modifiers) {
  if (!color.isValid()) {
    return;
  }
  current_ = color;
  emit Picked(color, modifiers);
  emit Changed();
}

void PaletteManager::Hover(const QColor& color) {
  if (color == hovered_) {
    return;
  }
  hovered_ = color;
  emit HoverChanged();
}

void PaletteManager::SetShown(bool is_shown) {
  if (is_shown == is_shown_) {
    return;
  }
  is_shown_ = is_shown;
  if (!is_shown_) {
    Hover(QColor());
    use_ = PaletteUse::kAuto;
  }
  emit ShownChanged(is_shown_);
}

void PaletteManager::SetUse(PaletteUse use) {
  assert(history_ != nullptr);
  assert(static_cast<int>(use) >= 0 && static_cast<int>(use) <= 2);
  const bool is_same = use_ == use;
  if (is_same) {
    return;
  }
  use_ = use;
  emit Changed();
}

bool PaletteManager::ShowsDoll() const {
  assert(history_ != nullptr);
  assert(selection_ != nullptr);
  const bool has_doll = !DollName().isEmpty();
  const bool is_free = use_ == PaletteUse::kFree;
  return has_doll && !is_free;
}

Project PaletteManager::WithRecent(Project project, const QColor& color) {
  assert(color.isValid());
  auto& recent = project.recent_colors;
  std::erase(recent, color);
  recent.insert(recent.begin(), color);
  if (recent.size() > static_cast<size_t>(kMaxRecentColors)) {
    recent.resize(static_cast<size_t>(kMaxRecentColors));
  }
  return project;
}

}  // namespace snapper
