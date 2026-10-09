// DragPlayhead: the playhead's place before a drag.

#include "ui/drag_playhead.h"

#include <cassert>

#include "edit/playback_manager.h"

namespace snapper {

DragPlayhead::DragPlayhead(PlaybackManager* playback)
    : playback_(playback) {
  assert(playback_ != nullptr);
  assert(!is_active_);
}

void DragPlayhead::Begin() {
  assert(playback_ != nullptr);
  assert(playback_->frame().index() >= 0);
  origin_ = playback_->frame();
  is_active_ = true;
}

bool DragPlayhead::Restore() {
  assert(playback_ != nullptr);
  assert(origin_.index() >= 0);
  const bool was_active = is_active_;
  is_active_ = false;
  if (was_active) {
    playback_->Seek(origin_);
  }
  return was_active;
}

void DragPlayhead::End() {
  assert(playback_ != nullptr);
  assert(origin_.index() >= 0);
  is_active_ = false;
}

}  // namespace snapper
