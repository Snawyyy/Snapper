#include "edit/settings_manager.h"

#include <algorithm>
#include <cassert>
#include <type_traits>
#include <variant>

#include "anim/sampler.h"
#include "base/text.h"
#include "edit/group_edits.h"
#include "edit/history_manager.h"
#include "edit/pose_edits.h"
#include "edit/project_edits.h"
#include "edit/rig_edits.h"

namespace snapper {
namespace {

// The drawing the piece track shows at local.
int DrawingAt(const Shot& shot, const Pick& pick, Frame local) {
  assert(local.index() >= 0);
  assert(!pick.piece.isEmpty());
  int drawing = -1;
  const TrackRef track{shot.id, TrackKind::kPiece, pick.layer, pick.piece};
  ReadTrack(shot, track, [&](const auto& channel) {
    using Held = typename std::remove_cvref_t<decltype(channel)>::value_type;
    constexpr bool is_pose = std::is_same_v<Held, PiecePose>;
    if constexpr (is_pose) {
      drawing = Sample(channel, local, PiecePose()).drawing;
    }
  });
  return drawing;
}

// The pieces of doll a pick stands for: the one picked, or all of them.
std::vector<const RigPiece*> PiecesOf(const Doll& doll, const Pick& pick) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(pick.layer.value() >= 0);
  std::vector<const RigPiece*> pieces;
  for (const RigPiece& rig : doll.rig.pieces) {
    const bool is_picked = pick.piece.isEmpty() || rig.name == pick.piece;
    if (is_picked) {
      pieces.push_back(&rig);
    }
  }
  return pieces;
}

// The settings of a text layer or an effect layer, as a clip part.
bool IsLookOnly(const Layer& layer) {
  assert(layer.id.value() >= 0);
  assert(layer.name.size() < 100000);
  return std::holds_alternative<TextLayer>(layer.content) ||
         std::holds_alternative<EffectLayer>(layer.content);
}

}  // namespace

SettingsManager::SettingsManager(HistoryManager* history)
    : history_(history) {
  assert(history_ != nullptr);
  assert(!clip_.has_value());
}

std::optional<SettingsManager::Clip> SettingsManager::Capture(
    const Project& project, ShotId shot, const Pick& pick, Frame local) {
  assert(local.index() >= 0);
  assert(shot.value() >= 0);
  const Shot* found = FindShot(project, shot);
  const Layer* layer =
      found != nullptr ? FindLayer(*found, pick.layer) : nullptr;
  const bool has_layer = layer != nullptr;
  if (!has_layer) {
    return std::nullopt;
  }
  Clip clip;
  clip.is_whole = pick.piece.isEmpty();
  const bool is_look = IsLookOnly(*layer);
  if (is_look) {
    const auto* text = std::get_if<TextLayer>(&layer->content);
    const auto* effect = std::get_if<EffectLayer>(&layer->content);
    const bool has_text = text != nullptr;
    const bool has_effect = effect != nullptr;
    if (has_text) {
      clip.text = *text;
    }
    if (has_effect) {
      clip.effect = std::pair(effect->kind, effect->color);
    }
    return clip;
  }
  const auto* posed = std::get_if<DollLayer>(&layer->content);
  const Doll* doll = DollOfLayer(project, shot, pick.layer);
  const bool is_doll = posed != nullptr && doll != nullptr;
  if (!is_doll) {
    return std::nullopt;
  }
  clip.doll = posed->doll;
  clip.is_flipped = posed->is_flipped;
  for (const RigPiece* rig : PiecesOf(*doll, pick)) {
    clip.pieces[rig->name] = {
        rig->warp, DotsOf(*rig, pick.layer),
        DrawingAt(*found, {pick.layer, rig->name}, local)};
  }
  for (const ShownGroup& group : ShownGroupsOf(project, shot)) {
    GroupClip kept{group.name, {}};
    for (const WarpDot& dot : group.dots) {
      const bool is_copied = !group.is_default && dot.layer == pick.layer &&
                             clip.pieces.contains(dot.piece);
      if (is_copied) {
        kept.dots.emplace_back(dot.piece, dot.point);
      }
    }
    const bool has_dots = !kept.dots.empty();
    if (has_dots) {
      clip.groups.push_back(std::move(kept));
    }
  }
  const bool has_pieces = !clip.pieces.empty();
  return has_pieces ? std::optional<Clip>(clip) : std::nullopt;
}

Result<void> SettingsManager::Copy(ShotId shot, const std::set<Pick>& picks,
                                   Frame local) {
  assert(history_ != nullptr);
  assert(local.index() >= 0);
  const QString problem = WhyNoCopy(shot, picks);
  const bool can_copy = problem.isEmpty();
  if (!can_copy) {
    return std::unexpected(Error{problem});
  }
  clip_ = Capture(history_->current(), shot, *picks.begin(), local);
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
  return is_picked ? QString()
                   : Tr("Pick a piece or layer on the stage first.");
}

QString SettingsManager::WhyNoPaste(ShotId shot,
                                    const std::set<Pick>& picks) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const bool has_clip = clip_.has_value();
  if (!has_clip) {
    return Tr("Copy a part's settings first (Ctrl+Shift+C).");
  }
  return WhyNoCopy(shot, picks);
}

Result<Project> SettingsManager::PasteGroups(Project project, ShotId shot,
                                             const Clip& clip,
                                             const Pick& target) {
  assert(shot.value() >= 0);
  assert(clip.groups.size() <= static_cast<size_t>(kMaxDotGroups) * 2);
  const std::vector<ShownGroup> now = ShownGroupsOf(project, shot);
  for (const GroupClip& group : clip.groups) {
    const auto old = std::find_if(
        now.begin(), now.end(),
        [&group](const ShownGroup& g) { return g.name == group.name; });
    const bool is_default = old != now.end() && old->is_default;
    if (is_default) {
      continue;
    }
    ShownGroup written{group.name, {}, false};
    const bool has_old = old != now.end();
    if (has_old) {
      written.dots = old->dots;
    }
    // The target's own dots in the group are replaced by the copy's.
    std::erase_if(written.dots, [&target](const WarpDot& dot) {
      return dot.layer == target.layer &&
             (target.piece.isEmpty() || dot.piece == target.piece);
    });
    for (const auto& [piece, point] : group.dots) {
      written.dots.insert(WarpDot{target.layer, piece, point});
    }
    auto next = WithGroupWritten(project, shot, group.name, written);
    const bool is_refused = !next;
    if (is_refused) {
      return next;
    }
    project = std::move(*next);
  }
  return project;
}

Result<Project> SettingsManager::PastePieces(Project project, ShotId shot,
                                             const Clip& clip,
                                             const Pick& target,
                                             Frame local) {
  assert(local.index() >= 0);
  assert(shot.value() >= 0);
  const Doll* doll = DollOfLayer(project, shot, target.layer);
  const DollLayer* posed = PosedLayerOf(project, shot, target.layer);
  const bool is_doll = doll != nullptr && posed != nullptr;
  if (!is_doll) {
    return std::unexpected(Error{Tr("Paste onto a doll or a piece of one.")});
  }
  const bool is_same_doll = posed->doll == clip.doll;
  const QString doll_name = posed->doll;
  for (const RigPiece* rig : PiecesOf(*doll, target)) {
    // A single copied piece goes onto any picked piece; a whole doll's
    // pieces go onto the pieces of the same name.
    const auto from = clip.is_whole ? clip.pieces.find(rig->name)
                                    : clip.pieces.begin();
    const bool is_found = from != clip.pieces.end();
    const bool is_fitting =
        is_found && from->second.warp.IsOn() && rig->warp == from->second.warp;
    if (!is_fitting) {
      continue;
    }
    const PieceDots dots = from->second.dots;
    const int drawing = from->second.drawing;
    const QString name = rig->name;
    const bool is_same_piece =
        is_same_doll && (clip.is_whole || from->first == name);
    auto next = WithPiece(project, doll_name, name, [&](RigPiece* piece) {
      piece->layer_dots[target.layer] = dots;
      return Result<void>();
    });
    const bool is_refused = !next;
    if (is_refused) {
      return next;
    }
    project = std::move(*next);
    // The same piece of the same doll also takes the drawing it shows.
    const bool is_drawn = is_same_piece && drawing >= 0;
    if (!is_drawn) {
      continue;
    }
    const TrackRef track{shot, TrackKind::kPiece, target.layer, name};
    auto drawn = KeyedAt(project, track, local, PiecePose(),
                         [drawing](PiecePose* pose) {
                           pose->drawing = drawing;
                           return Result<void>();
                         });
    const bool is_unkeyed = !drawn;
    if (is_unkeyed) {
      return drawn;
    }
    project = std::move(*drawn);
  }
  return project;
}

Result<Project> SettingsManager::PasteOnto(Project project, ShotId shot,
                                           const Clip& clip,
                                           const Pick& target, Frame local) {
  assert(local.index() >= 0);
  assert(shot.value() >= 0);
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
  const bool is_look = is_text || is_effect;
  if (is_look) {
    return WithLayer(std::move(project), shot, target.layer,
                     [&](Layer* edited) -> Result<void> {
                       if (is_text) {
                         auto& text = std::get<TextLayer>(edited->content);
                         const QString words = text.text;
                         text = *clip.text;
                         text.text = words;
                       } else {
                         auto& effect = std::get<EffectLayer>(edited->content);
                         effect.kind = clip.effect->first;
                         effect.color = clip.effect->second;
                       }
                       return Result<void>();
                     });
  }
  const bool is_doll = !clip.doll.isEmpty() &&
                       std::holds_alternative<DollLayer>(layer->content);
  if (!is_doll) {
    return std::unexpected(
        Error{Tr("Those settings are for another kind of thing.")});
  }
  const bool is_same_doll =
      std::get<DollLayer>(layer->content).doll == clip.doll;
  const bool is_whole_pair = clip.is_whole && target.piece.isEmpty();
  Result<Project> next = std::move(project);
  if (is_whole_pair) {
    next = WithLayer(std::move(*next), shot, target.layer,
                     [&](Layer* edited) -> Result<void> {
                       std::get<DollLayer>(edited->content).is_flipped =
                           clip.is_flipped;
                       return Result<void>();
                     });
  }
  if (next) {
    next = PastePieces(std::move(*next), shot, clip, target, local);
  }
  const bool wants_groups = next.has_value() && is_same_doll;
  if (wants_groups) {
    next = PasteGroups(std::move(*next), shot, clip, target);
  }
  return next;
}

Result<void> SettingsManager::Paste(ShotId shot, const std::set<Pick>& picks,
                                    Frame local) {
  assert(history_ != nullptr);
  assert(local.index() >= 0);
  const QString problem = WhyNoPaste(shot, picks);
  const bool can_paste = problem.isEmpty();
  if (!can_paste) {
    return std::unexpected(Error{problem});
  }
  Result<Project> next = history_->current();
  for (const Pick& target : picks) {
    next = PasteOnto(std::move(*next), shot, *clip_, target, local);
    const bool is_refused = !next;
    if (is_refused) {
      return std::unexpected(next.error());
    }
  }
  return history_->Apply(Tr("Paste settings"), std::move(next));
}

}  // namespace snapper
