// ReelManager's shot resizing: a shot clip's edge dragged on the reel
// changes the shot's own length.

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "anim/reel_timeline.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/reel_edits.h"
#include "edit/reel_manager.h"
#include "edit/shot_timing.h"

namespace snapper {
namespace {

// One resize: shots each get delta frames longer, at the start or the
// end; the clips in edged have their dragged edge move with it.
struct Resize final {
  std::set<ShotId> shots;
  std::set<ClipId> edged;
  int delta = 0;
  bool is_start = false;
  bool is_ripple = false;
  EdgeMode mode = EdgeMode::kHold;
};

const ShotSource* ShotOf(const Clip& clip) {
  assert(clip.length.index() >= 1);
  assert(clip.id.value() >= 0);
  return std::get_if<ShotSource>(&clip.source);
}

Result<void> ResizeOneShot(Project* project, ShotId id, const Resize& resize) {
  assert(project != nullptr);
  assert(std::abs(resize.delta) <= kMaxFrame);
  auto next = WithShot(*project, id, [&resize](Shot* shot) -> Result<void> {
    const int length = shot->length.index() + resize.delta;
    const bool is_in_range = length >= 1 && length <= kMaxFrame;
    if (!is_in_range) {
      return std::unexpected(Error{Tr("A shot is 1 to %1 frames long.")
                                       .arg(kMaxFrame)});
    }
    const bool is_stretch = resize.mode == EdgeMode::kStretch;
    if (is_stretch) {
      StretchShot(shot, Frame(length));
    } else if (resize.is_start) {
      ShiftShotFront(shot, resize.delta);
    } else {
      shot->length = Frame(length);
    }
    return {};
  });
  if (!next) {
    return std::unexpected(next.error());
  }
  *project = std::move(*next);
  return {};
}

// The clips of track after resize: full clips of a resized shot take
// its new length, edged ones move their dragged edge, and with ripple
// later clips move by what grew before them. olds maps each shot to its
// length before.
Result<void> ResizeTrackClips(ReelTrack* track, const Resize& resize,
                              const std::map<ShotId, int>& olds) {
  assert(track != nullptr);
  assert(track->clips.size() <= static_cast<size_t>(kMaxClipsPerTrack));
  const std::vector<Clip> was = track->clips;
  std::vector<std::pair<Frame, int>> growth;
  for (Clip& clip : track->clips) {
    const ShotSource* source = ShotOf(clip);
    const auto old = source != nullptr ? olds.find(source->shot) : olds.end();
    const bool is_resized = old != olds.end();
    if (!is_resized) {
      continue;
    }
    const bool is_full =
        clip.in.index() == 0 && clip.length.index() == old->second;
    const bool is_edged = resize.edged.contains(clip.id);
    const Frame end = clip.end();
    if (is_full) {
      clip.length = Frame(old->second + resize.delta);
    } else if (is_edged) {
      clip.length = Frame(std::max(1, clip.length.index() + resize.delta));
    } else {
      continue;
    }
    const bool moves_start = resize.is_start && is_edged && !resize.is_ripple;
    if (moves_start) {
      const int start = end.index() - clip.length.index();
      if (start < 0) {
        return std::unexpected(Error{Tr("Clips can't go past the reel.")});
      }
      clip.start = Frame(start);
    }
    growth.emplace_back(end, clip.end().index() - end.index());
  }
  if (resize.is_ripple) {
    for (size_t i = 0; i < track->clips.size(); ++i) {
      int shift = 0;
      for (const auto& [end, grew] : growth) {
        const bool is_after = !(was[i].start < end);
        shift += is_after ? grew : 0;
      }
      Clip& clip = track->clips[i];
      clip.start = Frame(std::max(0, clip.start.index() + shift));
    }
  }
  std::ranges::sort(track->clips, {}, &Clip::start);
  for (size_t i = 0; i + 1 < track->clips.size(); ++i) {
    const bool is_overlap = track->clips[i + 1].start < track->clips[i].end();
    if (is_overlap) {
      return std::unexpected(InTheWay());
    }
  }
  return {};
}

// project with resize done: the shots changed and the clips following.
Result<Project> Resized(Project project, const Resize& resize) {
  assert(resize.shots.size() <= static_cast<size_t>(kMaxShots));
  assert(std::abs(resize.delta) <= kMaxFrame);
  std::map<ShotId, int> olds;
  for (const ShotId id : resize.shots) {
    const Shot* shot = FindShot(project, id);
    if (shot == nullptr) {
      return std::unexpected(Error{Tr("That shot no longer exists.")});
    }
    olds[id] = shot->length.index();
    const auto done = ResizeOneShot(&project, id, resize);
    if (!done) {
      return std::unexpected(done.error());
    }
  }
  for (ReelTrack& track : project.reel.tracks) {
    const auto done = ResizeTrackClips(&track, resize, olds);
    if (!done) {
      return std::unexpected(done.error());
    }
  }
  return project;
}

// The most frames between 0 and resize.delta (> 0) that fit, found by
// halving: a longer resize only ever runs into more.
int MostThatFits(const Project& project, Resize resize) {
  assert(resize.delta > 0);
  assert(resize.delta <= kMaxFrame);
  int fits = 0;
  int fails = resize.delta;
  while (fails - fits > 1) {
    resize.delta = fits + (fails - fits) / 2;
    const bool is_fit = Resized(project, resize).has_value();
    fits = is_fit ? resize.delta : fits;
    fails = is_fit ? fails : resize.delta;
  }
  return fits;
}

}  // namespace

bool ReelManager::IsFullClip(ClipId clip) const {
  assert(history_ != nullptr);
  assert(clip.value() >= 0);
  const Project& project = history_->current();
  const Clip* found = ClipOf(project.reel, clip);
  const ShotSource* source = found != nullptr ? ShotOf(*found) : nullptr;
  const Shot* shot =
      source != nullptr ? FindShot(project, source->shot) : nullptr;
  return shot != nullptr && found->in.index() == 0 &&
         found->length == shot->length;
}

bool ReelManager::IsShotResize(ClipId clip, bool is_alt) const {
  assert(history_ != nullptr);
  assert(clip.value() >= 0);
  const Project& project = history_->current();
  const Clip* found = ClipOf(project.reel, clip);
  const ShotSource* source = found != nullptr ? ShotOf(*found) : nullptr;
  const bool has_shot =
      source != nullptr && FindShot(project, source->shot) != nullptr;
  return has_shot && IsFullClip(clip) != is_alt;
}

QString ReelManager::ResizeLabel(const std::vector<ClipId>& picked,
                                 ClipId clip) const {
  assert(history_ != nullptr);
  assert(clip.value() >= 0);
  const Project& project = history_->current();
  std::set<ShotId> shots;
  for (const ClipId id : picked) {
    const Clip* found = ClipOf(project.reel, id);
    const ShotSource* source = found != nullptr ? ShotOf(*found) : nullptr;
    if (source != nullptr) {
      shots.insert(source->shot);
    }
  }
  const Clip* grabbed = ClipOf(project.reel, clip);
  const ShotSource* source = grabbed != nullptr ? ShotOf(*grabbed) : nullptr;
  const Shot* shot =
      source != nullptr ? FindShot(project, source->shot) : nullptr;
  const bool is_many = shots.size() > 1;
  return is_many || shot == nullptr ? Tr("Resize shots")
                                    : Tr("Resize shot %1").arg(shot->name);
}

Result<ShotResize> ReelManager::ResizeShots(const std::vector<ClipId>& picked,
                                            ClipId clip, bool is_start,
                                            Frame edge, bool is_ripple) {
  assert(history_ != nullptr);
  assert(edge.index() >= 0);
  const Project& start = history_->before();
  const Clip* grabbed = ClipOf(start.reel, clip);
  const ShotSource* own = grabbed != nullptr ? ShotOf(*grabbed) : nullptr;
  const Shot* own_shot = own != nullptr ? FindShot(start, own->shot) : nullptr;
  if (own_shot == nullptr) {
    return std::unexpected(Error{Tr("Only a shot clip resizes its shot.")});
  }
  Resize resize{{own->shot}, {clip}, 0, is_start, is_ripple, edge_mode_};
  int shortest = own_shot->length.index();
  int longest = shortest;
  for (const ClipId id : picked) {
    const Clip* other = ClipOf(start.reel, id);
    const ShotSource* source = other != nullptr ? ShotOf(*other) : nullptr;
    const Shot* shot =
        source != nullptr ? FindShot(start, source->shot) : nullptr;
    if (shot != nullptr) {
      resize.shots.insert(shot->id);
      resize.edged.insert(id);
      shortest = std::min(shortest, shot->length.index());
      longest = std::max(longest, shot->length.index());
    }
  }
  const int asked = is_start ? grabbed->start.index() - edge.index()
                             : edge.index() - grabbed->end().index();
  resize.delta = std::clamp(asked, 1 - shortest, kMaxFrame - longest);
  QString stopped = resize.delta != asked
                        ? Tr("A shot is 1 to %1 frames long.").arg(kMaxFrame)
                        : QString();
  auto next = Resized(start, resize);
  const bool is_blocked = !next && resize.delta > 0;
  if (is_blocked) {
    stopped = next.error().message;
    resize.delta = MostThatFits(start, resize);
    next = Resized(start, resize);
  }
  if (!next) {
    return std::unexpected(next.error());
  }
  const Frame length = FindShot(*next, own->shot)->length;
  auto applied = history_->Apply(ResizeLabel(picked, clip), std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return ShotResize{resize.delta, length, stopped};
}

QString ReelManager::WhyNoFit(ClipId clip, Frame at) const {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const Project& project = history_->current();
  const Clip* found = ClipOf(project.reel, clip);
  const ShotSource* source = found != nullptr ? ShotOf(*found) : nullptr;
  const bool has_shot =
      source != nullptr && FindShot(project, source->shot) != nullptr;
  if (!has_shot) {
    return Tr("Only a shot clip can be fitted to a gap.");
  }
  const bool is_gap = SlotAt(project.reel, at).IsValid();
  return is_gap ? QString()
                : Tr("Mark a cut on both sides of the gap first (M).");
}

Result<void> ReelManager::FitShotToGap(ClipId clip, Frame at, EdgeMode mode) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const QString why_not = WhyNoFit(clip, at);
  if (!why_not.isEmpty()) {
    return std::unexpected(Error{why_not});
  }
  Project next = history_->current();
  const Slot slot = SlotAt(next.reel, at);
  const ClipSpot spot = FindClip(next.reel, clip);
  Clip& placed = TrackOf(&next, spot.track).clips[static_cast<size_t>(spot.index)];
  const Shot* shot = FindShot(next, ShotOf(placed)->shot);
  // The clip first shows its whole shot from the gap's start; the
  // resize then makes the shot as long as the gap.
  placed.start = slot.start;
  placed.in = Frame(0);
  placed.length = shot->length;
  const Resize resize{{shot->id}, {clip},
                      slot.length().index() - shot->length.index(),
                      false, false, mode};
  const QString label = Tr("Fit %1 to gap").arg(shot->name);
  return history_->Apply(label, Resized(std::move(next), resize));
}

}  // namespace snapper
