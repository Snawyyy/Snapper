// CompManager: adding a library entry, and the copied effect for Ctrl+C
// and Ctrl+V (view state, never saved).

#include <cassert>
#include <utility>
#include <vector>

#include "base/text.h"
#include "edit/comp_manager.h"
#include "edit/history_manager.h"

namespace snapper {

Result<CompEffectId> CompManager::AddEntry(ShotId shot,
                                           const CompEntry& entry,
                                           const CompTarget& target,
                                           int index) {
  assert(static_cast<int>(entry.kind) < kCompKindCount);
  assert(target.layer.IsValid() || target.pieces.empty());
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
  const CompEffect* effect = Find(shot, id);
  const bool is_gone = effect == nullptr;
  if (is_gone) {
    return std::unexpected(Error{Tr("That effect no longer exists.")});
  }
  copied_ = *effect;
  emit Changed();
  return {};
}

QString CompManager::CopiedName() const {
  assert(history_ != nullptr);
  assert(!copied_ || static_cast<int>(copied_->kind) < kCompKindCount);
  return copied_ ? EffectName(*copied_) : QString();
}

QString CompManager::WhyNoPaste(ShotId shot) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const bool has_copy = copied_.has_value();
  if (!has_copy) {
    return Tr("Copy an effect first (Ctrl+C).");
  }
  return WhyNoAdd(shot);
}

Result<CompEffectId> CompManager::Paste(ShotId shot, const CompTarget& target,
                                        int index) {
  assert(history_ != nullptr);
  assert(target.layer.IsValid() || target.pieces.empty());
  const QString why = WhyNoPaste(shot);
  const bool can_paste = why.isEmpty();
  if (!can_paste) {
    return std::unexpected(Error{why});
  }
  CompEffect effect = *copied_;
  effect.target.layer = target.layer;
  effect.target.pieces = target.pieces;
  const QString label = Tr("Paste %1").arg(EffectName(effect));
  return Insert(shot, std::move(effect), index, label);
}

}  // namespace snapper
