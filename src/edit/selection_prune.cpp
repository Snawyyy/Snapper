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
  const size_t counts_before =
      picks_.size() + keys_.size() + shots_.size() + clips_.size();
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
  std::erase_if(clips_, [&project](ClipId id) {
    return ClipOf(project.reel, id) == nullptr;
  });
  FixFocus();
  assert(!shot_.IsValid() || shots_.contains(shot_));
  const bool is_changed =
      shot_ != shot_before || focus_ != focus_before ||
      picks_.size() + keys_.size() + shots_.size() + clips_.size() !=
          counts_before;
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

bool SelectionManager::IsRealDot(const WarpDot& dot) const {
  assert(history_ != nullptr);
  assert(dot.point >= 0);
  const bool is_picked = picks_.contains({dot.layer, dot.piece});
  const Doll* doll = is_picked
                         ? DollOfLayer(history_->current(), shot_, dot.layer)
                         : nullptr;
  const RigPiece* rig =
      doll != nullptr ? FindRig(doll->rig, dot.piece) : nullptr;
  return rig != nullptr && dot.point < rig->warp.PointCount();
}

std::vector<WarpDot> SelectionManager::dots() const {
  assert(history_ != nullptr);
  assert(dots_.size() < 100000);
  std::vector<WarpDot> real;
  for (const WarpDot& dot : dots_) {
    const bool is_real = IsRealDot(dot);
    if (is_real) {
      real.push_back(dot);
    }
  }
  return real;
}

std::optional<WarpDot> SelectionManager::dot() const {
  assert(history_ != nullptr);
  assert(picks_.size() < 100000);
  const bool is_focus = dot_.has_value() && IsRealDot(*dot_);
  if (is_focus) {
    return dot_;
  }
  const std::vector<WarpDot> real = dots();
  return real.empty() ? std::nullopt : std::optional<WarpDot>(real.front());
}

void SelectionManager::PickDot(std::optional<WarpDot> dot) {
  assert(history_ != nullptr);
  assert(!dot || dot->point >= 0);
  PickDots(dot ? std::set<WarpDot>{*dot} : std::set<WarpDot>(),
           PickMode::kReplace);
}

void SelectionManager::PickDots(const std::set<WarpDot>& dots,
                                PickMode mode) {
  assert(history_ != nullptr);
  assert(dots.size() < 100000);
  const std::set<WarpDot> before = dots_;
  const std::optional<WarpDot> focus_before = dot_;
  dots_ = Combine(dots_, dots, mode);
  const bool is_focus_kept = dot_.has_value() && dots_.contains(*dot_);
  const bool is_focused = !dots.empty() && mode != PickMode::kRemove &&
                          dots_.contains(*dots.begin());
  if (is_focused) {
    dot_ = *dots.begin();
  } else if (!is_focus_kept) {
    dot_ = dots_.empty() ? std::nullopt
                         : std::optional<WarpDot>(*dots_.begin());
  }
  const bool is_changed = before != dots_ || focus_before != dot_;
  if (is_changed) {
    emit Changed();
  }
}

void SelectionManager::PickGroup(const std::set<WarpDot>& dots,
                                 PickMode mode) {
  assert(history_ != nullptr);
  assert(dots.size() < 100000);
  const bool has_dots = !dots.empty();
  if (!has_dots) {
    return;
  }
  const bool is_all_in = std::all_of(
      dots.begin(), dots.end(), [this](const WarpDot& dot) {
        return dots_.contains(dot) && IsRealDot(dot);
      });
  const bool is_taking_out =
      mode == PickMode::kRemove || (mode == PickMode::kToggle && is_all_in);
  const PickMode how =
      mode == PickMode::kReplace ? PickMode::kReplace : PickMode::kAdd;
  if (!is_taking_out) {
    std::set<Pick> pieces;
    for (const WarpDot& dot : dots) {
      pieces.insert({dot.layer, dot.piece});
    }
    PickThings(pieces, how, dots.begin()->layer);
  }
  PickDots(dots, is_taking_out ? PickMode::kRemove : how);
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
