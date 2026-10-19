#include "edit/palette_manager.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <variant>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"

namespace snapper {

PaletteManager::PaletteManager(HistoryManager* history,
                               SelectionManager* selection)
    : history_(history), selection_(selection) {
  assert(history_ != nullptr);
  assert(selection_ != nullptr);
  connect(history_, &HistoryManager::Changed, this, &PaletteManager::Changed);
  connect(selection_, &SelectionManager::Changed, this,
          &PaletteManager::Changed);
}

const Character* PaletteManager::PickedCharacter() const {
  assert(history_ != nullptr);
  assert(selection_ != nullptr);
  const Project& project = history_->current();
  const Shot* shot = FindShot(project, selection_->shot());
  const LayerId focus = selection_->layer();
  const Layer* layer =
      shot != nullptr && focus.IsValid() ? FindLayer(*shot, focus) : nullptr;
  const auto* cast =
      layer != nullptr ? std::get_if<CharacterLayer>(&layer->content) : nullptr;
  return cast != nullptr && !cast->character.isEmpty()
             ? FindCharacter(project, cast->character)
             : nullptr;
}

QString PaletteManager::CharacterName() const {
  assert(history_ != nullptr);
  const Character* character = PickedCharacter();
  return character != nullptr ? character->name : QString();
}

std::vector<QColor> PaletteManager::CharacterColors() const {
  assert(history_ != nullptr);
  const Character* character = PickedCharacter();
  if (character == nullptr) {
    return {};
  }
  std::map<QRgb, int> counts;
  for (const CharacterPart& part : character->parts) {
    for (const VectorShape& shape : part.drawing.shapes) {
      if (shape.has_fill) ++counts[shape.fill.rgba()];
      if (shape.has_stroke) ++counts[shape.stroke.rgba()];
    }
  }
  std::vector<std::pair<int, QRgb>> by_use;
  for (const auto& [rgb, count] : counts) by_use.emplace_back(-count, rgb);
  std::ranges::sort(by_use);
  std::vector<QColor> colors;
  for (const auto& [count, rgb] : by_use) colors.push_back(QColor::fromRgba(rgb));
  return colors;
}

void PaletteManager::SetRecentColors(std::vector<QColor> colors) {
  assert(colors.size() < 1000000);
  assert(kMaxRecentColors > 0);
  std::erase_if(colors, [](const QColor& color) { return !color.isValid(); });
  const auto most = static_cast<size_t>(kMaxRecentColors);
  if (colors.size() > most) {
    colors.resize(most);
  }
  if (colors == recent_) {
    return;
  }
  recent_ = std::move(colors);
  emit Changed();
}

void PaletteManager::Pick(const QColor& color,
                          Qt::KeyboardModifiers modifiers) {
  if (!color.isValid()) {
    return;
  }
  current_ = color;
  is_chosen_ = is_chosen_ || is_shown_;
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
  const bool is_closing = !is_shown_;
  const bool remembers = is_closing && is_chosen_;
  if (remembers) {
    std::vector<QColor> recent = recent_;
    std::erase(recent, current_);
    recent.insert(recent.begin(), current_);
    SetRecentColors(std::move(recent));
  }
  is_chosen_ = false;
  if (is_closing) {
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

bool PaletteManager::ShowsCharacter() const {
  assert(history_ != nullptr);
  assert(selection_ != nullptr);
  const bool has_character = PickedCharacter() != nullptr;
  const bool is_free = use_ == PaletteUse::kFree;
  return has_character && !is_free;
}

}  // namespace snapper
