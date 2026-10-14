// CompManager: what the Comp stage's pick means for effects, the target
// a new effect gets and the effects listed for it.

#include <algorithm>
#include <cassert>
#include <utility>
#include <variant>

#include "anim/comp_reel.h"
#include "base/text.h"
#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "model/project.h"

namespace snapper {
namespace {

// The most pieces a doll has; more is a broken pick.
constexpr size_t kMaxPicked = 4096;

// True when effect is listed for a pick of layer and pieces.
bool IsShown(const CompEffect& effect, LayerId layer,
             const std::set<QString>& pieces) {
  assert(pieces.size() <= kMaxPicked);
  assert(layer.IsValid() || pieces.empty());
  const bool is_screen_pick = !layer.IsValid();
  if (is_screen_pick) {
    return !effect.target.layer.IsValid();
  }
  const bool is_on_layer = effect.target.layer == layer;
  const bool is_layer_wide = effect.target.pieces.empty();
  const bool is_whole_pick = pieces.empty();
  const bool is_settled = !is_on_layer || is_layer_wide || is_whole_pick;
  if (is_settled) {
    return is_on_layer;
  }
  return std::ranges::any_of(effect.target.pieces, [&](const QString& one) {
    return pieces.contains(one);
  });
}

}  // namespace

QString CompManager::TargetText(ShotId shot, const CompTarget& target) const {
  assert(history_ != nullptr);
  assert(target.colors.size() <= static_cast<size_t>(kMaxCompColors));
  const Shot* found = FindShot(history_->current(), shot);
  QString text = Tr("Screen");
  const bool has_layer = target.layer.IsValid();
  if (has_layer) {
    const Layer* layer =
        found != nullptr ? FindLayer(*found, target.layer) : nullptr;
    text = layer != nullptr ? layer->name : Tr("A removed layer");
    const size_t pieces = target.pieces.size();
    const bool is_one = pieces == 1;
    const bool is_many = pieces > 1;
    if (is_one) {
      text += QStringLiteral(": ") + target.pieces.front();
    } else if (is_many) {
      text += Tr(": %1 pieces").arg(pieces);
    }
  }
  const size_t colors = target.colors.size();
  const bool has_colors = colors > 0;
  if (has_colors) {
    text += colors == 1 ? Tr(", 1 color") : Tr(", %1 colors").arg(colors);
  }
  return text;
}

QString CompManager::TargetTag(ShotId shot, const CompTarget& target) const {
  assert(history_ != nullptr);
  assert(target.pieces.size() <= kMaxPicked);
  const bool is_screen = !target.layer.IsValid();
  if (is_screen) {
    return Tr("(screen)");
  }
  const size_t pieces = target.pieces.size();
  const bool has_pieces = pieces > 0;
  if (has_pieces) {
    const bool is_one = pieces == 1;
    return is_one ? QStringLiteral("(%1)").arg(target.pieces.front())
                  : QStringLiteral("(%1 +%2)")
                        .arg(target.pieces.front())
                        .arg(pieces - 1);
  }
  const Shot* found = FindShot(history_->current(), shot);
  const Layer* layer =
      found != nullptr ? FindLayer(*found, target.layer) : nullptr;
  return QStringLiteral("(%1)").arg(layer != nullptr ? layer->name
                                                     : Tr("gone"));
}

Result<CompTarget> CompManager::TargetFor(
    ShotId shot, LayerId layer, const std::set<QString>& pieces) const {
  assert(history_ != nullptr);
  assert(pieces.size() <= kMaxPicked);
  const bool is_screen = !layer.IsValid();
  if (is_screen) {
    return CompTarget{};
  }
  const Shot* found = FindShot(history_->current(), shot);
  const Layer* picked = found != nullptr ? FindLayer(*found, layer) : nullptr;
  const bool is_gone = picked == nullptr;
  if (is_gone) {
    return std::unexpected(Error{Tr("The picked layer is gone.")});
  }
  const bool is_picture = IsStagePicture(*picked);
  if (!is_picture) {
    return std::unexpected(
        Error{Tr("An effect or storyboard layer draws nothing to work on.")});
  }
  CompTarget target;
  target.layer = layer;
  target.pieces.assign(pieces.begin(), pieces.end());
  return target;
}

std::vector<CompClipRef> CompManager::ShownFor(
    ShotId shot, LayerId layer, const std::set<QString>& pieces,
    Frame reel) const {
  assert(history_ != nullptr);
  assert(pieces.size() <= kMaxPicked);
  const Project& project = history_->current();
  std::vector<CompClipRef> shown;
  const bool is_screen_pick = !layer.IsValid();
  if (!is_screen_pick) {
    for (const CompEffect& effect : Of(shot)) {
      const bool is_shown = IsShown(effect, layer, pieces);
      if (is_shown) {
        shown.push_back(CompClipRef{shot, effect.id});
      }
    }
    return shown;
  }
  std::vector<std::pair<const Clip*, const Shot*>> shots;
  for (const auto& one : project.shots) {
    const Clip* clip = FirstClipOf(project, one->id);
    const bool is_listed = clip != nullptr || one->id == shot;
    if (is_listed) {
      shots.emplace_back(clip, one.get());
    }
  }
  std::ranges::stable_sort(shots, [](const auto& a, const auto& b) {
    const int a_at = a.first != nullptr ? a.first->start.index() : -1;
    const int b_at = b.first != nullptr ? b.first->start.index() : -1;
    return a_at < b_at;
  });
  for (const auto& [clip, one] : shots) {
    for (const CompEffect& effect : one->comp) {
      const bool is_screen = !effect.target.layer.IsValid();
      const ReelSpan span =
          clip != nullptr ? EffectOnReel(*clip, effect) : ReelSpan{};
      const bool is_running =
          reel.index() >= span.first && reel.index() < span.end;
      const bool is_shown = is_screen && (one->id == shot || is_running);
      if (is_shown) {
        shown.push_back(CompClipRef{one->id, effect.id});
      }
    }
  }
  return shown;
}

}  // namespace snapper
