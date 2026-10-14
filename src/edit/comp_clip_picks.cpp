// CompManager: the effect clips picked on the Comp timeline (view
// state, never saved).

#include <cassert>
#include <set>

#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "model/project.h"

namespace snapper {

std::set<CompClipRef> CompManager::AllClips() const {
  assert(history_ != nullptr);
  const Project& project = history_->current();
  assert(project.shots.size() < 100000);
  std::set<CompClipRef> clips;
  for (const auto& shot : project.shots) {
    for (const CompEffect& effect : shot->comp) {
      clips.insert(CompClipRef{shot->id, effect.id});
    }
  }
  return clips;
}

CompClipRef CompManager::Focus() const {
  assert(history_ != nullptr);
  assert(focus_.id.value() >= 0);
  const bool is_there = Find(focus_.shot, focus_.id) != nullptr;
  return is_there ? focus_ : CompClipRef();
}

std::set<CompClipRef> CompManager::ClipPicks() const {
  assert(history_ != nullptr);
  assert(clip_picks_.size() < 100000);
  std::set<CompClipRef> real;
  for (const CompClipRef& clip : clip_picks_) {
    const bool is_there = Find(clip.shot, clip.id) != nullptr;
    if (is_there) {
      real.insert(clip);
    }
  }
  return real;
}

void CompManager::PickClips(const std::set<CompClipRef>& clips,
                            PickMode mode, CompClipRef focus) {
  assert(history_ != nullptr);
  assert(clips.size() < 100000);
  const std::set<CompClipRef> next = Combine(ClipPicks(), clips, mode);
  const bool is_focus_picked = next.contains(focus);
  const bool is_same = next == clip_picks_ &&
                       (!is_focus_picked || focus_ == focus);
  if (is_same) {
    return;
  }
  clip_picks_ = next;
  if (is_focus_picked) {
    picked_[focus.shot] = focus.id;
    focus_ = focus;
  }
  emit Changed();
}

}  // namespace snapper
