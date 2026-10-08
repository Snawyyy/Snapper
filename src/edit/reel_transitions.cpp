// ReelManager's transitions: how one clip hands over to the next one
// touching it on the same track.

#include <cassert>
#include <utility>

#include "anim/reel_timeline.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/reel_edits.h"
#include "edit/reel_manager.h"

namespace snapper {

QString TransitionName(TransitionKind kind) {
  assert(static_cast<int>(kind) < kTransitionKindCount);
  assert(kTransitionKindCount == 7);
  switch (kind) {
    case TransitionKind::kCut:
      return Tr("Cut");
    case TransitionKind::kSwipeLeft:
      return Tr("Swipe left");
    case TransitionKind::kSwipeRight:
      return Tr("Swipe right");
    case TransitionKind::kSwipeUp:
      return Tr("Swipe up");
    case TransitionKind::kSwipeDown:
      return Tr("Swipe down");
    case TransitionKind::kFlash:
      return Tr("Flash");
    case TransitionKind::kCrossfade:
      return Tr("Crossfade");
  }
  return QString();
}

QString ReelManager::WhyNoTransition(ClipId clip) const {
  assert(history_ != nullptr);
  assert(clip.value() >= 0);
  const Reel& reel = history_->current().reel;
  const ClipSpot spot = FindClip(reel, clip);
  const bool is_present = spot.IsValid();
  if (!is_present) {
    return Tr("That clip no longer exists.");
  }
  const ReelTrack& track = reel.tracks[static_cast<size_t>(spot.track)];
  const Clip& found = track.clips[static_cast<size_t>(spot.index)];
  const bool is_joined = NextTouching(track, found) != nullptr;
  if (!is_joined) {
    return Tr("A transition needs a clip starting right where this "
              "one ends.");
  }
  const bool has_room = LongestTransition(track, found).index() >= 1;
  return has_room ? QString()
                  : Tr("Both clips need two frames or more for a "
                       "transition.");
}

Result<void> ReelManager::SetTransition(ClipId clip,
                                        Transition transition) {
  assert(history_ != nullptr);
  assert(static_cast<int>(transition.kind) < kTransitionKindCount);
  const QString why_not = WhyNoTransition(clip);
  const bool can_set = why_not.isEmpty();
  if (!can_set) {
    return std::unexpected(Error{why_not});
  }
  const Reel& reel = history_->current().reel;
  const ClipSpot spot = FindClip(reel, clip);
  const Frame longest =
      LongestTransition(reel.tracks[static_cast<size_t>(spot.track)],
                        *ClipOf(reel, clip));
  const bool is_cut = transition.kind == TransitionKind::kCut;
  const bool fits = is_cut || (transition.length.index() >= 1 &&
                               transition.length <= longest);
  if (!fits) {
    return std::unexpected(Error{
        Tr("A transition here runs 1 to %1 frames.").arg(longest.index())});
  }
  const Transition kept = is_cut ? Transition() : transition;
  return history_->Apply(
      Tr("Change transition"),
      WithClip(history_->current(), clip, [kept](Clip* edited, int*) {
        edited->out = kept;
        return Result<void>();
      }));
}

}  // namespace snapper
