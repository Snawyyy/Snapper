// StoryboardManager: the edits of a storyboard layer's settings and
// cels, and the cel pick (see storyboard_manager.h).

#include <algorithm>
#include <cassert>
#include <cmath>

#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/storyboard_edits.h"
#include "edit/storyboard_manager.h"

namespace snapper {
namespace {

QString CelLabel(const char* one, const char* many, size_t count) {
  assert(one != nullptr && many != nullptr);
  assert(count > 0);
  return count == 1 ? Tr(one) : Tr(many).arg(count);
}

}  // namespace

Result<void> StoryboardManager::SetOpacity(ShotId shot, LayerId layer,
                                           double opacity) {
  assert(history_ != nullptr);
  assert(std::isfinite(opacity));
  const double clamped = std::clamp(opacity, 0.0, 1.0);
  return history_->Apply(
      Tr("Storyboard opacity"),
      WithBoard(history_->current(), shot, layer,
                [clamped](StoryboardLayer* board) -> Result<void> {
                  board->opacity = clamped;
                  return {};
                }));
}

Result<void> StoryboardManager::SetExported(ShotId shot, LayerId layer,
                                            bool is_exported) {
  assert(history_ != nullptr);
  assert(layer.value() >= 0);
  return history_->Apply(
      is_exported ? Tr("Export storyboard")
                  : Tr("Leave storyboard out of export"),
      WithBoard(history_->current(), shot, layer,
                [is_exported](StoryboardLayer* board) -> Result<void> {
                  board->is_exported = is_exported;
                  return {};
                }));
}

Result<void> StoryboardManager::MoveCels(ShotId shot, LayerId layer,
                                         const std::set<Frame>& frames,
                                         int offset) {
  assert(history_ != nullptr);
  assert(frames.size() <= static_cast<size_t>(kMaxStoryboardCels));
  const bool has_none = frames.empty();
  if (has_none) {
    return std::unexpected(Error{Tr("Pick a storyboard frame first.")});
  }
  auto applied = history_->Apply(
      CelLabel("Move storyboard frame", "Move %1 storyboard frames",
               frames.size()),
      WithBoard(history_->before(), shot, layer,
                [&frames, offset](StoryboardLayer* board) {
                  return ::snapper::MoveCels(board, frames, offset);
                }));
  if (applied) {
    std::set<Frame> moved;
    for (const Frame frame : frames) {
      moved.insert(Frame(frame.index() + offset));
    }
    picked_shot_ = shot;
    picked_layer_ = layer;
    picked_ = std::move(moved);
    emit Changed();
  }
  return applied;
}

Result<void> StoryboardManager::DeleteCels(ShotId shot, LayerId layer,
                                           const std::set<Frame>& frames) {
  assert(history_ != nullptr);
  assert(frames.size() <= static_cast<size_t>(kMaxStoryboardCels));
  const bool has_none = frames.empty();
  if (has_none) {
    return std::unexpected(Error{Tr("Pick a storyboard frame first.")});
  }
  auto applied = history_->Apply(
      CelLabel("Delete storyboard frame", "Delete %1 storyboard frames",
               frames.size()),
      WithBoard(history_->current(), shot, layer,
                [&frames](StoryboardLayer* board) {
                  return RemoveCels(board, frames);
                }));
  if (applied) {
    ClearPick();
  }
  return applied;
}

QString StoryboardManager::WhyNoDelete(ShotId shot) const {
  assert(history_ != nullptr);
  assert(picked_.size() <= static_cast<size_t>(kMaxStoryboardCels) + 1);
  const bool has_pick = !Picked(shot, Active(shot)).empty();
  return has_pick ? QString()
                  : Tr("Pick a storyboard frame on the timeline first.");
}

std::set<Frame> StoryboardManager::Picked(ShotId shot, LayerId layer) const {
  assert(history_ != nullptr);
  assert(layer.value() >= 0);
  const StoryboardLayer* board = Of(shot, layer);
  std::set<Frame> picked;
  const bool is_ours =
      board != nullptr && shot == picked_shot_ && layer == picked_layer_;
  if (!is_ours) {
    return picked;
  }
  for (const Frame frame : picked_) {
    const bool exists = CelStartingAt(*board, frame) != nullptr;
    if (exists) {
      picked.insert(frame);
    }
  }
  return picked;
}

void StoryboardManager::Pick(ShotId shot, LayerId layer,
                             const std::set<Frame>& frames, PickMode mode) {
  assert(frames.size() <= static_cast<size_t>(kMaxStoryboardCels));
  assert(layer.IsValid());
  const std::set<Frame> was = Picked(shot, layer);
  picked_shot_ = shot;
  picked_layer_ = layer;
  picked_ = Combine(was, frames, mode);
  emit Changed();
}

void StoryboardManager::ClearPick() {
  assert(picked_.size() <= static_cast<size_t>(kMaxStoryboardCels) + 1);
  assert(picked_shot_.value() >= 0);
  const bool was_empty = picked_.empty();
  picked_.clear();
  picked_shot_ = ShotId();
  picked_layer_ = LayerId();
  if (!was_empty) {
    emit Changed();
  }
}

}  // namespace snapper
