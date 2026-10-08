// ReelManager's edits that touch many clips or whole tracks.

#include <cassert>
#include <set>
#include <utility>
#include <vector>

#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/reel_manager.h"

namespace snapper {
namespace {

struct Lifted final {
  int track = -1;
  Clip clip;
};

// Takes every clip in ids off its track, or says which is gone.
Result<std::vector<Lifted>> Lift(Project* project,
                                 const std::set<ClipId>& ids) {
  assert(project != nullptr);
  assert(ids.size() <= static_cast<size_t>(kMaxReelTracks) *
                           kMaxClipsPerTrack);
  std::vector<Lifted> lifted;
  for (const ClipId id : ids) {
    const ClipSpot spot = FindClip(project->reel, id);
    const bool is_present = spot.IsValid();
    if (!is_present) {
      return std::unexpected(Error{Tr("That clip no longer exists.")});
    }
    auto& clips = project->reel.tracks[static_cast<size_t>(spot.track)].clips;
    lifted.push_back({spot.track, clips[static_cast<size_t>(spot.index)]});
    clips.erase(clips.begin() + spot.index);
  }
  return lifted;
}

}  // namespace

Result<void> ReelManager::MoveAll(const std::vector<ClipId>& clips,
                                  int tracks, int frames) {
  assert(history_ != nullptr);
  assert(frames >= -kMaxFrame && frames <= kMaxFrame);
  const std::set<ClipId> ids(clips.begin(), clips.end());
  Project next = history_->before();
  auto lifted = Lift(&next, ids);
  if (!lifted) {
    return std::unexpected(lifted.error());
  }
  const int track_count = static_cast<int>(next.reel.tracks.size());
  for (Lifted& item : *lifted) {
    const int track = item.track + tracks;
    const int start = item.clip.start.index() + frames;
    const bool is_on_reel = track >= 0 && track < track_count && start >= 0;
    if (!is_on_reel) {
      return std::unexpected(Error{Tr("Clips can't go past the reel.")});
    }
    item.clip.start = Frame(start);
    ReelTrack& row = next.reel.tracks[static_cast<size_t>(track)];
    const bool has_room = HasRoom(row, item.clip);
    if (!has_room) {
      return std::unexpected(Error{Tr("There's a clip in the way.")});
    }
    PlaceClip(&row, std::move(item.clip));
  }
  return history_->Apply(ids.size() == 1 ? Tr("Move clip") : Tr("Move clips"),
                         std::move(next));
}

Result<void> ReelManager::RemoveAll(const std::vector<ClipId>& clips) {
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
  return history_->Apply(
      ids.size() == 1 ? Tr("Remove clip") : Tr("Remove clips"),
      std::move(next));
}

Result<void> ReelManager::AddTrack() {
  assert(history_ != nullptr);
  Project next = history_->current();
  const bool is_full =
      next.reel.tracks.size() >= static_cast<size_t>(kMaxReelTracks);
  if (is_full) {
    return std::unexpected(
        Error{Tr("The video holds at most %1 tracks.").arg(kMaxReelTracks)});
  }
  next.reel.tracks.emplace_back();
  assert(next.reel.tracks.back().clips.empty());
  return history_->Apply(Tr("Add track"), std::move(next));
}

Result<void> ReelManager::RemoveTrack(int track) {
  assert(history_ != nullptr);
  assert(track >= -1);
  Project next = history_->current();
  const int count = static_cast<int>(next.reel.tracks.size());
  const bool is_track = track >= 0 && track < count;
  if (!is_track) {
    return std::unexpected(Error{Tr("That track no longer exists.")});
  }
  const bool can_go =
      count > 1 && next.reel.tracks[static_cast<size_t>(track)].clips.empty();
  if (!can_go) {
    return std::unexpected(
        Error{Tr("Only an empty track can go, and one must stay.")});
  }
  next.reel.tracks.erase(next.reel.tracks.begin() + track);
  return history_->Apply(Tr("Remove track"), std::move(next));
}

}  // namespace snapper
