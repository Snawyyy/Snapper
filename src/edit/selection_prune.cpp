// SelectionManager's upkeep: dropping what no longer exists.

#include <algorithm>
#include <cassert>

#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"

namespace snapper {

void SelectionManager::FixFocus() {
  assert(picks_.size() < 100000);
  const bool is_focus_picked =
      std::any_of(picks_.begin(), picks_.end(),
                  [this](const Pick& pick) { return pick.layer == focus_; });
  if (!is_focus_picked) {
    focus_ = picks_.empty() ? LayerId() : picks_.begin()->layer;
  }
  assert(picks_.empty() || focus_.IsValid());
}

void SelectionManager::Prune() {
  assert(history_ != nullptr);
  const Project& project = history_->current();
  const ShotId shot_before = shot_;
  const LayerId focus_before = focus_;
  const size_t counts_before = picks_.size() + keys_.size() + shots_.size();
  std::erase_if(shots_, [&project](ShotId id) {
    return FindShot(project, id) == nullptr;
  });
  const bool is_shot_gone = FindShot(project, shot_) == nullptr;
  if (is_shot_gone) {
    // Fall back to a picked shot, else the first, so there is always
    // somewhere to work.
    shot_ = !shots_.empty()           ? *shots_.begin()
            : project.shots.empty() ? ShotId()
                                    : project.shots.front()->id;
    picks_.clear();
  }
  const bool has_shot = shot_.IsValid();
  if (has_shot) {
    shots_.insert(shot_);
  }
  std::erase_if(picks_, [this](const Pick& pick) { return !IsReal(pick); });
  std::erase_if(keys_, [this](const KeyRef& key) { return !IsKey(key); });
  FixFocus();
  const bool is_changed =
      shot_ != shot_before || focus_ != focus_before ||
      picks_.size() + keys_.size() + shots_.size() != counts_before;
  if (is_changed) {
    emit Changed();
  }
}

bool SelectionManager::IsReal(const Pick& pick) const {
  assert(history_ != nullptr);
  assert(pick.layer.value() >= 0);
  const Shot* shot = FindShot(history_->current(), shot_);
  const bool has_layer = shot != nullptr && pick.layer.IsValid() &&
                         FindLayer(*shot, pick.layer) != nullptr;
  if (!has_layer) {
    return false;
  }
  const bool is_whole = pick.piece.isEmpty();
  if (is_whole) {
    return true;
  }
  const Doll* doll = DollOfLayer(history_->current(), shot_, pick.layer);
  return doll != nullptr && FindRig(doll->rig, pick.piece) != nullptr;
}

std::optional<WarpDot> SelectionManager::dot() const {
  assert(history_ != nullptr);
  assert(picks_.size() < 100000);
  const bool is_picked =
      dot_.has_value() && picks_.contains({dot_->layer, dot_->piece});
  const Doll* doll = is_picked
                         ? DollOfLayer(history_->current(), shot_, dot_->layer)
                         : nullptr;
  const RigPiece* rig =
      doll != nullptr ? FindRig(doll->rig, dot_->piece) : nullptr;
  const bool is_on_grid =
      rig != nullptr && dot_->point < rig->warp.PointCount();
  return is_on_grid ? dot_ : std::nullopt;
}

void SelectionManager::PickDot(std::optional<WarpDot> dot) {
  assert(history_ != nullptr);
  assert(!dot || dot->point >= 0);
  const bool is_changed = dot != dot_;
  dot_ = std::move(dot);
  if (is_changed) {
    emit Changed();
  }
}

bool SelectionManager::IsKey(const KeyRef& key) const {
  assert(history_ != nullptr);
  assert(key.frame.index() >= 0);
  const Shot* shot = FindShot(history_->current(), key.track.shot);
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return false;
  }
  bool is_key = false;
  ReadTrack(*shot, key.track, [&](const auto& channel) {
    is_key = KeyIndexAt(channel, key.frame) >= 0;
  });
  return is_key;
}

}  // namespace snapper
