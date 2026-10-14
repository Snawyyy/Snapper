// CompManager: the Comp tab's view state (the pick, V, K and what the
// stage's last frame cost), never saved.

#include <algorithm>
#include <cassert>
#include <set>

#include "base/frame.h"
#include "base/text.h"
#include "edit/comp_manager.h"

namespace snapper {
namespace {

// A frame at 24 fps, in milliseconds.
constexpr double kFrameBudget = 1000.0 / kFramesPerSecond;

// The effects that took longer than a frame.
std::set<CompEffectId> SlowOf(const std::map<CompEffectId, double>& costs) {
  assert(kFrameBudget > 0.0);
  assert(std::ranges::all_of(
      costs, [](const auto& cost) { return cost.second >= 0.0; }));
  std::set<CompEffectId> ids;
  for (const auto& [id, ms] : costs) {
    const bool is_slow = ms > kFrameBudget;
    if (is_slow) {
      ids.insert(id);
    }
  }
  return ids;
}

}  // namespace

CompEffectId CompManager::PickedIn(ShotId shot) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const auto found = picked_.find(shot);
  const bool is_picked =
      found != picked_.end() && Find(shot, found->second) != nullptr;
  return is_picked ? found->second : CompEffectId();
}

void CompManager::Pick(ShotId shot, CompEffectId id) {
  assert(shot.value() >= 0);
  assert(id.value() >= 0);
  const std::set<CompClipRef> alone =
      id.IsValid() ? std::set<CompClipRef>{{shot, id}}
                   : std::set<CompClipRef>();
  const bool is_same = PickedIn(shot) == id && clip_picks_ == alone &&
                       focus_ == CompClipRef{shot, id};
  if (is_same) {
    return;
  }
  picked_[shot] = id;
  clip_picks_ = alone;
  focus_ = CompClipRef{shot, id};
  emit Changed();
}

void CompManager::SetEffectsOff(bool is_off) {
  assert(history_ != nullptr);
  const bool is_same = are_effects_off_ == is_off;
  if (is_same) {
    return;
  }
  are_effects_off_ = is_off;
  assert(are_effects_off_ == is_off);
  emit Changed();
}

void CompManager::SetMaskShown(bool is_shown) {
  assert(history_ != nullptr);
  const bool is_same = is_mask_shown_ == is_shown;
  if (is_same) {
    return;
  }
  is_mask_shown_ = is_shown;
  assert(is_mask_shown_ == is_shown);
  emit Changed();
}

CompShow CompManager::ShowFor(ShotId shot) const {
  assert(history_ != nullptr);
  CompShow show;
  show.is_off = are_effects_off_;
  const CompClipRef focus = Focus();
  show.mask_of = is_mask_shown_ ? focus.id : CompEffectId();
  show.mask_shot = is_mask_shown_ ? focus.shot : ShotId();
  assert(shot.value() >= 0);
  assert(!show.mask_of.IsValid() || is_mask_shown_);
  return show;
}

void CompManager::NoteCosts(const CompReport& report) {
  assert(history_ != nullptr);
  // Every run effect has a cost, so a failed one has one too.
  assert(report.failed.size() <= report.costs.size());
  const bool is_changed =
      SlowOf(report.costs) != SlowOf(costs_) || report.failed != failed_;
  costs_ = report.costs;
  failed_ = report.failed;
  if (is_changed) {
    emit Changed();
  }
}

bool CompManager::IsSlow(CompEffectId id) const {
  assert(id.value() >= 0);
  assert(kFrameBudget > 0.0);
  const auto found = costs_.find(id);
  return found != costs_.end() && found->second > kFrameBudget;
}

QString CompManager::WhyNotRun(const CompEffect& effect) const {
  assert(static_cast<int>(effect.kind) < kCompKindCount);
  const bool is_plugin = effect.kind == CompKind::kFsp;
  if (!is_plugin) {
    return QString();
  }
  const QString why = WhyNoPlugin(effect.plugin);
  const bool is_missing = !why.isEmpty();
  if (is_missing) {
    return why;
  }
  const bool has_failed = failed_.contains(effect.id);
  assert(!has_failed || costs_.contains(effect.id));
  return has_failed
             ? Tr("%1 could not run with these values on this frame, so it "
                  "is skipped. Try other values.")
                   .arg(EffectName(effect))
             : QString();
}

}  // namespace snapper
