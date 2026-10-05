#include "edit/selection_manager.h"

#include <cassert>
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

void SelectionManager::SelectShot(ShotId shot) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const bool is_known = FindShot(history_->current(), shot) != nullptr;
  const bool is_new = is_known && shot != shot_;
  if (!is_new) {
    return;
  }
  shot_ = shot;
  layer_ = LayerId();
  pieces_.clear();
  keys_.clear();
  emit Changed();
}

void SelectionManager::SelectLayer(LayerId layer) {
  assert(history_ != nullptr);
  assert(layer.value() >= 0);
  const Shot* shot = FindShot(history_->current(), shot_);
  const bool is_known =
      !layer.IsValid() || (shot != nullptr && FindLayer(*shot, layer));
  const bool is_new = is_known && layer != layer_;
  if (!is_new) {
    return;
  }
  layer_ = layer;
  pieces_.clear();
  emit Changed();
}

void SelectionManager::SelectPieces(const std::set<QString>& pieces,
                                    bool add) {
  assert(history_ != nullptr);
  assert(pieces.size() <= static_cast<size_t>(kMaxPieceTracks));
  std::set<QString> next = add ? pieces_ : std::set<QString>();
  for (const QString& piece : pieces) {
    const bool is_real = IsPiece(piece);
    if (is_real) {
      next.insert(piece);
    }
  }
  const bool is_changed = next != pieces_;
  if (is_changed) {
    pieces_ = std::move(next);
    emit Changed();
  }
}

void SelectionManager::SelectKeys(const std::set<KeyRef>& keys, bool add) {
  assert(history_ != nullptr);
  assert(keys_.size() < 1000000);
  std::set<KeyRef> next = add ? keys_ : std::set<KeyRef>();
  for (const KeyRef& key : keys) {
    const bool is_real = IsKey(key);
    if (is_real) {
      next.insert(key);
    }
  }
  const bool is_changed = next != keys_;
  if (is_changed) {
    keys_ = std::move(next);
    emit Changed();
  }
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

void SelectionManager::Clear() {
  assert(history_ != nullptr);
  const bool had_any = layer_.IsValid() || !pieces_.empty() || !keys_.empty();
  layer_ = LayerId();
  pieces_.clear();
  keys_.clear();
  assert(!layer_.IsValid());
  if (had_any) {
    emit Changed();
  }
}

void SelectionManager::Prune() {
  assert(history_ != nullptr);
  const Project& project = history_->current();
  const ShotId shot_before = shot_;
  const LayerId layer_before = layer_;
  const size_t counts_before = pieces_.size() + keys_.size();
  const bool is_shot_gone = FindShot(project, shot_) == nullptr;
  if (is_shot_gone) {
    // Fall back to the first shot, so there is always somewhere to work.
    shot_ = project.shots.empty() ? ShotId() : project.shots.front()->id;
    layer_ = LayerId();
  }
  const Shot* shot = FindShot(project, shot_);
  const bool is_layer_gone =
      layer_.IsValid() &&
      (shot == nullptr || FindLayer(*shot, layer_) == nullptr);
  if (is_layer_gone) {
    layer_ = LayerId();
  }
  std::erase_if(pieces_, [this](const QString& p) { return !IsPiece(p); });
  std::erase_if(keys_, [this](const KeyRef& k) { return !IsKey(k); });
  const bool is_changed = shot_ != shot_before || layer_ != layer_before ||
                          pieces_.size() + keys_.size() != counts_before;
  assert(pieces_.empty() || layer_.IsValid());
  if (is_changed) {
    emit Changed();
  }
}

bool SelectionManager::IsPiece(const QString& piece) const {
  assert(history_ != nullptr);
  assert(!piece.isEmpty());
  const Doll* doll = DollOfLayer(history_->current(), shot_, layer_);
  return doll != nullptr && FindRig(doll->rig, piece) != nullptr;
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
