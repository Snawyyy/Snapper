// ReelManager's cuts: trimming, splitting and ripple deleting clips.

#include <algorithm>
#include <cassert>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "anim/reel_timeline.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/reel_edits.h"
#include "edit/reel_manager.h"

namespace snapper {
namespace {

// Slides each clip left on track by the length of every clip in gone
// that came before it, closing the gaps they left.
void CloseGaps(ReelTrack* track, const std::vector<Clip>& gone) {
  assert(track != nullptr);
  assert(gone.size() <= static_cast<size_t>(kMaxClipsPerTrack));
  for (Clip& clip : track->clips) {
    int slide = 0;
    for (const Clip& removed : gone) {
      const bool is_before = removed.start < clip.start;
      slide += is_before ? removed.length.index() : 0;
    }
    clip.start = Frame(clip.start.index() - slide);
  }
}

}  // namespace

Result<void> ReelManager::TrimStart(ClipId clip, Frame start) {
  assert(history_ != nullptr);
  assert(start.index() >= 0);
  return history_->Apply(
      Tr("Trim clip"),
      WithClip(history_->current(), clip,
               [start](Clip* edited, int*) -> Result<void> {
                 const int shift = start.index() - edited->start.index();
                 const int in = edited->in.index() + shift;
                 const int length = edited->length.index() - shift;
                 const bool is_valid = in >= 0 && length >= 1;
                 if (!is_valid) {
                   return std::unexpected(
                       Error{Tr("The clip has nothing more to show there.")});
                 }
                 edited->start = start;
                 edited->in = Frame(in);
                 edited->length = Frame(length);
                 return {};
               }));
}

Result<void> ReelManager::TrimEnd(ClipId clip, Frame end) {
  assert(history_ != nullptr);
  assert(end.index() >= 0);
  const Project& project = history_->current();
  return history_->Apply(
      Tr("Trim clip"),
      WithClip(project, clip,
               [end, &project](Clip* edited, int*) -> Result<void> {
                 const int length = end.index() - edited->start.index();
                 const int left = SourceLength(project, *edited).index() -
                                  edited->in.index();
                 // Shortening always works, even past a shot that has
                 // since got shorter; lengthening stops at the source.
                 const bool is_valid =
                     length >= 1 &&
                     (length <= edited->length.index() || length <= left);
                 if (!is_valid) {
                   return std::unexpected(
                       Error{Tr("The clip has nothing more to show there.")});
                 }
                 edited->length = Frame(length);
                 return {};
               }));
}

Result<ClipId> ReelManager::Split(ClipId clip, Frame at) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  Project next = history_->current();
  auto right = SplitIn(&next, clip, at);
  if (!right) {
    return std::unexpected(right.error());
  }
  auto applied = history_->Apply(Tr("Split clip"), std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return *right;
}

std::vector<ClipId> ReelManager::SplitTargets(
    const std::vector<ClipId>& clips, Frame at) const {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const bool has_pick = !clips.empty();
  if (has_pick) {
    return clips;
  }
  // Nothing picked: whatever the playhead is inside, on every track.
  std::vector<ClipId> under;
  for (const ReelTrack& track : history_->current().reel.tracks) {
    for (const Clip& clip : track.clips) {
      const bool is_inside = clip.start < at && at < clip.end();
      if (is_inside) {
        under.push_back(clip.id);
      }
    }
  }
  return under;
}

QString ReelManager::WhyNoSplit(const std::vector<ClipId>& clips,
                                Frame at) const {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const std::vector<ClipId> targets = SplitTargets(clips, at);
  const Reel& reel = history_->current().reel;
  const bool is_any_inside =
      std::ranges::any_of(targets, [&reel, at](ClipId id) {
        const Clip* clip = ClipOf(reel, id);
        return clip != nullptr && clip->start < at && at < clip->end();
      });
  const bool has_pick = !clips.empty();
  return is_any_inside ? QString()
         : has_pick    ? Tr("Put the playhead inside a picked clip.")
                       : Tr("Put the playhead inside a clip.");
}

Result<std::vector<ClipId>> ReelManager::SplitAll(
    const std::vector<ClipId>& clips, Frame at) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const QString why_not = WhyNoSplit(clips, at);
  const bool can_split = why_not.isEmpty();
  if (!can_split) {
    return std::unexpected(Error{why_not});
  }
  Project next = history_->current();
  std::vector<ClipId> halves;
  for (const ClipId clip : SplitTargets(clips, at)) {
    auto right = SplitIn(&next, clip, at);
    if (right) {
      halves.push_back(*right);
    }
  }
  auto applied = history_->Apply(
      CountLabel(halves.size(), Tr("Split clip"), Tr("Split clips")),
      std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return halves;
}

Result<void> ReelManager::RippleDeleteAll(const std::vector<ClipId>& clips) {
  assert(history_ != nullptr);
  assert(clips.size() < 1000000);
  const std::set<ClipId> ids(clips.begin(), clips.end());
  const bool is_empty = ids.empty();
  if (is_empty) {
    return std::unexpected(Error{Tr("Pick a clip first.")});
  }
  Project next = history_->current();
  auto lifted = Lift(&next, ids);
  if (!lifted) {
    return std::unexpected(lifted.error());
  }
  std::map<int, std::vector<Clip>> gone;
  for (const Lifted& item : *lifted) {
    gone[item.track].push_back(item.clip);
  }
  for (const auto& [track, removed] : gone) {
    CloseGaps(&TrackOf(&next, track), removed);
  }
  return history_->Apply(CountLabel(ids.size(), Tr("Ripple remove clip"),
                                    Tr("Ripple remove clips")),
                         std::move(next));
}

}  // namespace snapper
