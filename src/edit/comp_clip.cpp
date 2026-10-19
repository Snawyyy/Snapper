// CompManager: adding a library entry, and the copied effect for Ctrl+C
// and Ctrl+V (view state, never saved).

#include <cassert>
#include <set>
#include <utility>
#include <vector>

#include "base/text.h"
#include "edit/comp_manager.h"
#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "model/project.h"

namespace snapper {

Result<CompEffectId> CompManager::AddEntry(ShotId shot,
                                           const CompEntry& entry,
                                           const CompTarget& target,
                                           int index) {
  assert(static_cast<int>(entry.kind) < kCompKindCount);
  assert(target.layer.value() >= 0);
  const bool is_scene = entry.is_scene_gradient;
  const bool is_plugin = entry.kind == CompKind::kFsp;
  const Result<CompEffectId> added =
      is_scene    ? AddSceneGradient(shot, target, index)
      : is_plugin ? AddPlugin(shot, entry.plugin, target, index)
                  : Add(shot, entry.kind, target, index);
  const bool is_added = added.has_value();
  if (is_added) {
    std::vector<CompEntry> recent = recent_entries_;
    std::erase(recent, entry);
    recent.insert(recent.begin(), entry);
    SetRecentEntries(std::move(recent));
  }
  return added;
}

void CompManager::SetRecentEntries(std::vector<CompEntry> entries) {
  assert(history_ != nullptr);
  assert(kMaxRecentEntries > 0);
  const auto most = static_cast<size_t>(kMaxRecentEntries);
  const bool is_long = entries.size() > most;
  if (is_long) {
    entries.resize(most);
  }
  const bool is_same = entries == recent_entries_;
  if (is_same) {
    return;
  }
  recent_entries_ = std::move(entries);
  emit Changed();
}

Result<void> CompManager::Copy(ShotId shot, CompEffectId id) {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  return CopyClips({CompClipRef{shot, id}});
}

Result<void> CompManager::CopyClips(const std::set<CompClipRef>& clips) {
  assert(history_ != nullptr);
  assert(clips.size() < 100000);
  std::vector<Copied> copies;
  for (const CompClipRef& clip : clips) {
    const CompEffect* effect = Find(clip.shot, clip.id);
    const bool is_there = effect != nullptr;
    if (is_there) {
      copies.push_back(Copied{*effect, clip.shot, FramesOf(clip)});
    }
  }
  const bool is_none = copies.empty();
  if (is_none) {
    return std::unexpected(Error{Tr("Pick an effect to copy it.")});
  }
  copied_ = std::move(copies);
  emit Changed();
  return {};
}

QString CompManager::CopiedName() const {
  assert(history_ != nullptr);
  assert(copied_.size() < 100000);
  const bool is_one = copied_.size() == 1;
  if (is_one) {
    return EffectName(copied_.front().effect);
  }
  return copied_.empty() ? QString()
                         : Tr("%1 effects").arg(copied_.size());
}

QString CompManager::WhyNoPaste(ShotId shot) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const bool has_copy = !copied_.empty();
  if (!has_copy) {
    return Tr("Copy an effect first (Ctrl+C).");
  }
  const Shot* found = FindShot(history_->current(), shot);
  const bool fits =
      found == nullptr ||
      found->comp.size() + copied_.size() <=
          static_cast<size_t>(kMaxCompEffectsPerShot);
  return fits ? WhyNoAdd(shot)
              : Tr("A shot holds at most %1 effects.")
                    .arg(kMaxCompEffectsPerShot);
}

Result<CompEffectId> CompManager::Paste(ShotId shot, const CompTarget& target,
                                        int index) {
  assert(history_ != nullptr);
  assert(target.layer.value() >= 0);
  const QString why = WhyNoPaste(shot);
  const bool can_paste = why.isEmpty();
  if (!can_paste) {
    return std::unexpected(Error{why});
  }
  const std::vector<Copied> copies = copied_;
  EditScope scope(history_, Tr("Paste %1").arg(CopiedName()));
  Result<CompEffectId> made = CompEffectId();
  for (size_t i = 0; i < copies.size() && made.has_value(); ++i) {
    CompEffect effect = copies[i].effect;
    effect.target.layer = target.layer;
    const bool is_top = index < 0;
    const int at = is_top ? kOnTop : index + static_cast<int>(i);
    made = Insert(shot, std::move(effect), at, Tr("Paste effect"));
  }
  return made;
}

}  // namespace snapper
