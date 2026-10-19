#include "edit/selection_manager.h"

#include <algorithm>
#include <cassert>
#include <utility>
#include <variant>

#include "edit/history_manager.h"
#include "edit/project_edits.h"

namespace snapper {

SelectionManager::SelectionManager(HistoryManager* history)
    : history_(history) {
  assert(history_ != nullptr);
  connect(history_, &HistoryManager::Changed, this,
          &SelectionManager::Prune);
  Prune();
  assert(keys_.empty());
}

std::vector<LayerId> SelectionManager::PickedLayers() const {
  assert(picks_.size() < 100000);
  std::vector<LayerId> layers;
  for (const Pick& pick : picks_) {
    const bool is_new =
        std::find(layers.begin(), layers.end(), pick.layer) == layers.end();
    if (is_new) {
      layers.push_back(pick.layer);
    }
  }
  assert(layers.size() <= picks_.size());
  return layers;
}

std::vector<TrackRef> SelectionManager::PickedTracks() const {
  assert(picks_.size() < 100000);
  std::vector<TrackRef> tracks;
  for (const Pick& pick : picks_) {
    tracks.push_back(TrackRef{shot_, TrackKind::kLayer, pick.layer});
  }
  assert(tracks.size() == picks_.size());
  return tracks;
}

void SelectionManager::SelectShot(ShotId shot) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const bool is_known = FindShot(history_->current(), shot) != nullptr;
  const bool is_new = is_known && (shot != shot_ || shots_.size() > 1);
  if (!is_new) {
    return;
  }
  shot_ = shot;
  shots_ = {shot};
  focus_ = LayerId();
  picks_.clear();
  keys_.clear();
  emit Changed();
}

void SelectionManager::PickShots(const std::set<ShotId>& shots,
                                 PickMode mode) {
  assert(history_ != nullptr);
  assert(shots.size() <= static_cast<size_t>(kMaxShots));
  std::set<ShotId> next = Combine(shots_, shots, mode);
  std::erase_if(next, [this](ShotId id) {
    return FindShot(history_->current(), id) == nullptr;
  });
  const bool is_empty = next.empty();
  if (is_empty) {
    next = {shot_};
  }
  const bool keeps_focus = next.contains(shot_);
  const ShotId focus = keeps_focus ? shot_ : *next.begin();
  const bool is_changed = next != shots_ || focus != shot_;
  if (!is_changed) {
    return;
  }
  const bool is_new_focus = focus != shot_;
  if (is_new_focus) {
    focus_ = LayerId();
    picks_.clear();
    keys_.clear();
  }
  shot_ = focus;
  shots_ = std::move(next);
  emit Changed();
}

void SelectionManager::SelectLayer(LayerId layer) {
  assert(history_ != nullptr);
  assert(layer.value() >= 0);
  const bool is_clear = !layer.IsValid();
  if (is_clear) {
    PickThings({}, PickMode::kReplace, LayerId());
    return;
  }
  const bool is_unknown = !IsReal(Pick{layer});
  const bool is_same =
      focus_ == layer && picks_ == std::set<Pick>{Pick{layer}};
  const bool is_new = !is_unknown && !is_same;
  if (is_new) {
    PickThings({Pick{layer}}, PickMode::kReplace, layer);
  }
}

void SelectionManager::PickThings(const std::set<Pick>& picks, PickMode mode,
                                  LayerId focus) {
  assert(history_ != nullptr);
  assert(picks.size() < 100000);
  std::set<Pick> real;
  for (const Pick& pick : picks) {
    const bool is_real = pick.layer.IsValid() && IsReal(pick);
    if (is_real) {
      real.insert(pick);
    }
  }
  std::set<Pick> next = Combine(picks_, real, mode);
  const LayerId before = focus_;
  const bool is_changed = next != picks_;
  picks_ = std::move(next);
  focus_ = focus;
  FixFocus();
  const bool is_news = is_changed || focus_ != before;
  if (is_news) {
    emit Changed();
  }
}

void SelectionManager::SelectKeys(const std::set<KeyRef>& keys, bool add) {
  assert(history_ != nullptr);
  assert(keys.size() < 1000000);
  PickKeys(keys, add ? PickMode::kAdd : PickMode::kReplace);
}

void SelectionManager::PickKeys(const std::set<KeyRef>& keys, PickMode mode) {
  assert(history_ != nullptr);
  assert(keys_.size() < 1000000);
  std::set<KeyRef> real;
  for (const KeyRef& key : keys) {
    const bool is_real = IsKey(key);
    if (is_real) {
      real.insert(key);
    }
  }
  std::set<KeyRef> next = Combine(keys_, real, mode);
  const bool is_changed = next != keys_;
  if (is_changed) {
    keys_ = std::move(next);
    emit Changed();
  }
}

std::set<Frame> SelectionManager::EditFrames(Frame at) const {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  std::set<Frame> frames;
  for (const KeyRef& key : keys_) {
    const bool is_here = key.track.shot == shot_;
    if (is_here) {
      frames.insert(key.frame);
    }
  }
  const bool has_picked = !frames.empty();
  return has_picked ? frames : std::set<Frame>{at};
}

void SelectionManager::ClearKeys() {
  assert(history_ != nullptr);
  assert(keys_.size() < 1000000);
  const bool had_keys = !keys_.empty();
  keys_.clear();
  if (had_keys) {
    emit Changed();
  }
}

void SelectionManager::PickClips(const std::set<ClipId>& clips,
                                 PickMode mode) {
  assert(history_ != nullptr);
  assert(clips.size() < 1000000);
  std::set<ClipId> next = Combine(clips_, clips, mode);
  std::erase_if(next, [this](ClipId id) {
    return ClipOf(history_->current().reel, id) == nullptr;
  });
  const bool is_changed = next != clips_;
  clips_ = std::move(next);
  if (is_changed) {
    emit Changed();
  }
}

void SelectionManager::Clear() {
  assert(history_ != nullptr);
  const bool had_any =
      focus_.IsValid() || !picks_.empty() || !keys_.empty();
  focus_ = LayerId();
  picks_.clear();
  keys_.clear();
  assert(!focus_.IsValid());
  if (had_any) {
    emit Changed();
  }
}

}  // namespace snapper
