#include "edit/reel_manager.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

#include "anim/reel_timeline.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/reel_edits.h"
#include "media/video_reader.h"

namespace snapper {
namespace {

// Cuts away whatever is on track between slot's ends, splitting clips
// that cross them, so a clip can be laid over the gap.
void ClearSlot(Project* project, int track, Slot slot) {
  assert(project != nullptr);
  assert(slot.IsValid());
  const bool is_track = IsTrack(*project, track);
  if (!is_track) {
    return;
  }
  // Split at both ends first, so every clip is wholly in or out.
  for (const Frame edge : {slot.start, slot.end}) {
    for (const Clip& clip : TrackOf(project, track).clips) {
      const bool is_crossing = clip.start < edge && edge < clip.end();
      if (is_crossing) {
        const auto made = SplitIn(project, clip.id, edge);
        assert(made.has_value());
        break;
      }
    }
  }
  auto& clips = TrackOf(project, track).clips;
  std::erase_if(clips, [slot](const Clip& clip) {
    return clip.start >= slot.start && clip.end() <= slot.end;
  });
}

}  // namespace

ReelManager::ReelManager(HistoryManager* history) : history_(history) {
  assert(history_ != nullptr);
  assert(!history_->IsScopeOpen());
}

Result<VideoSource> ReelManager::ReadVideo(const QString& path) const {
  assert(history_ != nullptr);
  assert(kFramesPerSecond > 0);
  const auto info = ProbeVideo(path);
  if (!info) {
    return std::unexpected(info.error());
  }
  const Frame length(
      static_cast<int>(std::lround(info->seconds * kFramesPerSecond)));
  return VideoSource{path, length};
}

Result<ClipId> ReelManager::AddVideo(const QString& path, int track,
                                     Frame start) {
  assert(history_ != nullptr);
  assert(track >= -1);
  const auto video = ReadVideo(path);
  if (!video) {
    return std::unexpected(video.error());
  }
  const Frame length = video->length;
  ClipId id;
  auto applied = history_->Apply(
      Tr("Add video"),
      WithNewClip(history_->current(), *video, track, start, length, &id));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return id;
}

Result<ClipId> ReelManager::FillSlot(int track, Frame at,
                                     const ClipSource& source, Frame in) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const Project& now = history_->current();
  const Slot slot = SlotAt(now.reel, at);
  const bool is_slot = slot.IsValid();
  if (!is_slot) {
    return std::unexpected(
        Error{Tr("Mark a cut on both sides of the gap first (M).")});
  }
  const int last = LastSlotIn(now, source, slot);
  const bool is_long_enough = last >= 0;
  if (!is_long_enough) {
    return std::unexpected(Error{Tr("That video is shorter than the gap.")});
  }
  const Frame from(std::clamp(in.index(), 0, last));
  const Clip* filling = SlotClip(now.reel, track, slot);
  const bool is_refill = filling != nullptr;
  ClipId id = is_refill ? filling->id : ClipId();
  Project cleared = now;
  ClearSlot(&cleared, track, slot);
  auto next =
      is_refill
          ? WithClip(now, id, [&](Clip* clip, int*) {
              clip->source = source;
              return Result<void>();
            })
          : WithNewClip(std::move(cleared), source, track, slot.start,
                        slot.length(), &id);
  const bool is_placed = next.has_value();
  if (is_placed) {
    const ClipSpot spot = FindClip(next->reel, id);
    TrackOf(&*next, spot.track).clips[static_cast<size_t>(spot.index)].in =
        from;
  }
  auto applied = history_->Apply(Tr("Fill gap"), std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return id;
}

Result<ClipId> ReelManager::AddShot(ShotId shot, int track, Frame start) {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const Shot* found = FindShot(history_->current(), shot);
  const bool is_present = found != nullptr;
  if (!is_present) {
    return std::unexpected(Error{Tr("That shot no longer exists.")});
  }
  ClipId id;
  auto applied = history_->Apply(
      Tr("Add shot to video"),
      WithNewClip(history_->current(), ShotSource{shot}, track, start,
                  found->length, &id));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return id;
}

QString ReelManager::WhyNoAddTrack() const {
  assert(history_ != nullptr);
  assert(kMaxReelTracks > 0);
  const bool is_full = history_->current().reel.tracks.size() >=
                       static_cast<size_t>(kMaxReelTracks);
  return is_full ? Tr("The video holds at most %1 tracks.").arg(kMaxReelTracks)
                 : QString();
}

QString ReelManager::WhyNoRemoveTrack(int track) const {
  assert(history_ != nullptr);
  assert(track >= -1);
  const Reel& reel = history_->current().reel;
  const int count = static_cast<int>(reel.tracks.size());
  const bool is_track = track >= 0 && track < count;
  if (!is_track) {
    return Tr("That track no longer exists.");
  }
  const bool can_go =
      count > 1 && reel.tracks[static_cast<size_t>(track)].clips.empty();
  return can_go ? QString()
                : Tr("Only an empty track can go, and one must stay.");
}

}  // namespace snapper
