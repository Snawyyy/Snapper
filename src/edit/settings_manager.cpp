#include "edit/settings_manager.h"

#include <cassert>
#include <variant>

#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"

namespace snapper {

SettingsManager::SettingsManager(HistoryManager* history)
    : history_(history) {
  assert(history_ != nullptr);
  assert(!clip_.has_value());
}

std::optional<SettingsManager::Clip> SettingsManager::Capture(
    const Project& project, ShotId shot, const Pick& pick) {
  assert(shot.value() >= 0);
  assert(pick.layer.value() >= 0);
  const Shot* found = FindShot(project, shot);
  const Layer* layer =
      found != nullptr ? FindLayer(*found, pick.layer) : nullptr;
  const bool has_layer = layer != nullptr;
  if (!has_layer) {
    return std::nullopt;
  }
  Clip clip;
  const auto* text = std::get_if<TextLayer>(&layer->content);
  const auto* effect = std::get_if<EffectLayer>(&layer->content);
  const auto* character = std::get_if<CharacterLayer>(&layer->content);
  if (text != nullptr) {
    clip.text = *text;
  } else if (effect != nullptr) {
    clip.effect = std::pair(effect->kind, effect->color);
  } else if (character != nullptr) {
    clip.is_flipped = character->is_flipped;
  } else {
    return std::nullopt;
  }
  return clip;
}

Result<void> SettingsManager::Copy(ShotId shot, const std::set<Pick>& picks) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const QString problem = WhyNoCopy(shot, picks);
  const bool can_copy = problem.isEmpty();
  if (!can_copy) {
    return std::unexpected(Error{problem});
  }
  clip_ = Capture(history_->current(), shot, *picks.begin());
  const bool is_copied = clip_.has_value();
  if (!is_copied) {
    return std::unexpected(Error{Tr("That has no settings to copy.")});
  }
  return {};
}

QString SettingsManager::WhyNoCopy(ShotId shot,
                                   const std::set<Pick>& picks) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const bool has_shot = FindShot(history_->current(), shot) != nullptr;
  const bool is_picked = has_shot && !picks.empty();
  return is_picked ? QString() : Tr("Pick a layer on the stage first.");
}

QString SettingsManager::WhyNoPaste(ShotId shot,
                                    const std::set<Pick>& picks) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const bool has_clip = clip_.has_value();
  if (!has_clip) {
    return Tr("Copy a layer's settings first (Ctrl+Shift+C).");
  }
  return WhyNoCopy(shot, picks);
}

Result<Project> SettingsManager::PasteOnto(Project project, ShotId shot,
                                           const Clip& clip,
                                           const Pick& target) {
  assert(shot.value() >= 0);
  assert(target.layer.value() >= 0);
  const Shot* found = FindShot(project, shot);
  const Layer* layer =
      found != nullptr ? FindLayer(*found, target.layer) : nullptr;
  const bool has_layer = layer != nullptr;
  if (!has_layer) {
    return std::unexpected(Error{Tr("That layer no longer exists.")});
  }
  const bool is_text = clip.text.has_value() &&
                       std::holds_alternative<TextLayer>(layer->content);
  const bool is_effect = clip.effect.has_value() &&
                         std::holds_alternative<EffectLayer>(layer->content);
  const bool is_character =
      clip.is_flipped.has_value() &&
      std::holds_alternative<CharacterLayer>(layer->content);
  const bool is_fitting = is_text || is_effect || is_character;
  if (!is_fitting) {
    return std::unexpected(
        Error{Tr("Those settings are for another kind of thing.")});
  }
  return WithLayer(std::move(project), shot, target.layer,
                   [&](Layer* edited) -> Result<void> {
                     if (is_text) {
                       auto& text = std::get<TextLayer>(edited->content);
                       const QString words = text.text;
                       text = *clip.text;
                       text.text = words;
                     } else if (is_effect) {
                       auto& effect = std::get<EffectLayer>(edited->content);
                       effect.kind = clip.effect->first;
                       effect.color = clip.effect->second;
                     } else {
                       std::get<CharacterLayer>(edited->content).is_flipped =
                           *clip.is_flipped;
                     }
                     return Result<void>();
                   });
}

Result<void> SettingsManager::Paste(ShotId shot,
                                    const std::set<Pick>& picks) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const QString problem = WhyNoPaste(shot, picks);
  const bool can_paste = problem.isEmpty();
  if (!can_paste) {
    return std::unexpected(Error{problem});
  }
  Result<Project> next = history_->current();
  for (const Pick& target : picks) {
    next = PasteOnto(std::move(*next), shot, *clip_, target);
    const bool is_refused = !next;
    if (is_refused) {
      return std::unexpected(next.error());
    }
  }
  return history_->Apply(Tr("Paste settings"), std::move(next));
}

}  // namespace snapper
