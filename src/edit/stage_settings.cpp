// StageManager's layer settings.

#include <cassert>
#include <cmath>
#include <variant>

#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/stage_manager.h"

namespace snapper {

Result<void> StageManager::Rename(ShotId shot, LayerId layer,
                                  const QString& name) {
  assert(history_ != nullptr);
  assert(name.size() < 100000);
  const bool is_blank = name.trimmed().isEmpty();
  if (is_blank) {
    return std::unexpected(Error{Tr("A layer needs a name.")});
  }
  return history_->Apply(
      Tr("Rename layer"),
      WithLayer(history_->current(), shot, layer, [&name](Layer* edited) {
        edited->name = name.trimmed();
        return Result<void>();
      }));
}

Result<void> StageManager::SetVisible(ShotId shot, LayerId layer,
                                      bool is_visible) {
  assert(history_ != nullptr);
  assert(layer.value() >= 0);
  return history_->Apply(
      is_visible ? Tr("Show layer") : Tr("Hide layer"),
      WithLayer(history_->current(), shot, layer,
                [is_visible](Layer* edited) {
                  edited->is_visible = is_visible;
                  return Result<void>();
                }));
}

Result<void> StageManager::SetRange(ShotId shot, LayerId layer, Frame start,
                                    Frame length) {
  assert(history_ != nullptr);
  assert(start.index() >= 0 && length.index() >= 0);
  return history_->Apply(
      Tr("Change layer timing"),
      WithLayer(history_->current(), shot, layer,
                [start, length](Layer* edited) {
                  edited->start = start;
                  edited->length = length;
                  return Result<void>();
                }));
}

Result<void> StageManager::SetFlipped(ShotId shot, LayerId layer,
                                      bool is_flipped) {
  assert(history_ != nullptr);
  assert(layer.value() >= 0);
  return history_->Apply(
      Tr("Flip doll"),
      WithLayer(history_->current(), shot, layer,
                [is_flipped](Layer* edited) {
                  auto* doll = std::get_if<DollLayer>(&edited->content);
                  const bool is_doll = doll != nullptr;
                  if (!is_doll) {
                    return Result<void>(std::unexpected(
                        Error{Tr("Only dolls can be flipped.")}));
                  }
                  doll->is_flipped = is_flipped;
                  return Result<void>();
                }));
}

Result<void> StageManager::SetText(ShotId shot, LayerId layer,
                                   const TextLayer& text) {
  assert(history_ != nullptr);
  assert(layer.value() >= 0);
  const bool is_valid = !text.text.trimmed().isEmpty() && text.size > 0.0 &&
                        text.outline_width >= 0.0 && text.fill.isValid() &&
                        text.outline.isValid();
  if (!is_valid) {
    return std::unexpected(Error{
        Tr("Text needs words, a size above zero and real colours.")});
  }
  return history_->Apply(
      Tr("Edit text"),
      WithLayer(history_->current(), shot, layer, [&text](Layer* edited) {
        auto* words = std::get_if<TextLayer>(&edited->content);
        const bool is_text = words != nullptr;
        if (!is_text) {
          return Result<void>(
              std::unexpected(Error{Tr("That layer isn't text.")}));
        }
        *words = text;
        return Result<void>();
      }));
}

Result<void> StageManager::SetEffect(ShotId shot, LayerId layer,
                                     EffectKind kind, QColor color) {
  assert(history_ != nullptr);
  assert(static_cast<int>(kind) < kEffectKindCount);
  const bool is_valid = color.isValid();
  if (!is_valid) {
    return std::unexpected(Error{Tr("That isn't a colour.")});
  }
  return history_->Apply(
      Tr("Change effect"),
      WithLayer(history_->current(), shot, layer,
                [kind, color](Layer* edited) {
                  auto* effect = std::get_if<EffectLayer>(&edited->content);
                  const bool is_effect = effect != nullptr;
                  if (!is_effect) {
                    return Result<void>(std::unexpected(
                        Error{Tr("That layer isn't an effect.")}));
                  }
                  effect->kind = kind;
                  effect->color = color;
                  return Result<void>();
                }));
}

}  // namespace snapper
