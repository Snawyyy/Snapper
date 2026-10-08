// ReelManager's clipboard: copy, cut, paste and duplicate clips.

#include <algorithm>
#include <cassert>
#include <set>
#include <utility>
#include <vector>

#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/reel_edits.h"
#include "edit/reel_manager.h"

namespace snapper {
namespace {

// Clips copied off the reel with their tracks, sorted by start and
// counted from the first one's; end is where the last one ended.
struct Copies final {
  std::vector<Lifted> clips;
  Frame end;
};

Result<Copies> CopiesOf(const Reel& reel, const std::vector<ClipId>& clips) {
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(clips.size() < 1000000);
  const std::set<ClipId> ids(clips.begin(), clips.end());
  const bool is_empty = ids.empty();
  if (is_empty) {
    return std::unexpected(Error{Tr("Pick a clip first.")});
  }
  Copies copies;
  for (const ClipId id : ids) {
    const ClipSpot spot = FindClip(reel, id);
    const bool is_present = spot.IsValid();
    if (!is_present) {
      return std::unexpected(Error{Tr("That clip no longer exists.")});
    }
    copies.clips.push_back({spot.track, *ClipOf(reel, id)});
  }
  std::ranges::sort(copies.clips, [](const Lifted& a, const Lifted& b) {
    return a.clip.start < b.clip.start;
  });
  const int first = copies.clips.front().clip.start.index();
  for (Lifted& copy : copies.clips) {
    copies.end = std::max(copies.end, copy.clip.end());
    copy.clip.start = Frame(copy.clip.start.index() - first);
  }
  return copies;
}

// The project with copies laid down from at, their new ids in made.
Result<Project> WithCopies(Project project, const std::vector<Lifted>& copies,
                           Frame at, std::vector<ClipId>* made) {
  assert(made != nullptr);
  assert(at.index() >= 0);
  const Result<void> placed = PlaceCopies(&project, copies, at, made);
  if (!placed) {
    return std::unexpected(placed.error());
  }
  return project;
}

// The project with copies of clips laid right after the last of them.
Result<Project> WithDuplicates(const Project& project,
                               const std::vector<ClipId>& clips,
                               std::vector<ClipId>* made) {
  assert(made != nullptr);
  assert(clips.size() < 1000000);
  const auto copies = CopiesOf(project.reel, clips);
  if (!copies) {
    return std::unexpected(copies.error());
  }
  return WithCopies(project, copies->clips, copies->end, made);
}

}  // namespace

QString ReelManager::WhyNoPick(const std::vector<ClipId>& clips) const {
  assert(history_ != nullptr);
  assert(clips.size() < 1000000);
  const auto copies = CopiesOf(history_->current().reel, clips);
  return copies ? QString() : copies.error().message;
}

Result<void> ReelManager::Copy(const std::vector<ClipId>& clips) {
  assert(history_ != nullptr);
  assert(clips.size() < 1000000);
  auto copies = CopiesOf(history_->current().reel, clips);
  if (!copies) {
    return std::unexpected(copies.error());
  }
  clipboard_ = std::move(copies->clips);
  assert(!clipboard_.empty());
  return {};
}

Result<void> ReelManager::CutAll(const std::vector<ClipId>& clips) {
  assert(history_ != nullptr);
  assert(clips.size() < 1000000);
  const Result<void> copied = Copy(clips);
  if (!copied) {
    return copied;
  }
  Project next = history_->current();
  const std::set<ClipId> ids(clips.begin(), clips.end());
  auto lifted = Lift(&next, ids);
  if (!lifted) {
    return std::unexpected(lifted.error());
  }
  return history_->Apply(
      CountLabel(ids.size(), Tr("Cut clip"), Tr("Cut clips")),
      std::move(next));
}

QString ReelManager::WhyNoPaste(Frame at) const {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const bool is_empty = clipboard_.empty();
  if (is_empty) {
    return Tr("Copy a clip first.");
  }
  std::vector<ClipId> made;
  const auto pasted = WithCopies(history_->current(), clipboard_, at, &made);
  return pasted ? QString() : pasted.error().message;
}

Result<std::vector<ClipId>> ReelManager::Paste(Frame at) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const QString why_not = WhyNoPaste(at);
  const bool can_paste = why_not.isEmpty();
  if (!can_paste) {
    return std::unexpected(Error{why_not});
  }
  std::vector<ClipId> made;
  auto applied = history_->Apply(
      CountLabel(clipboard_.size(), Tr("Paste clip"), Tr("Paste clips")),
      WithCopies(history_->current(), clipboard_, at, &made));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return made;
}

QString ReelManager::WhyNoDuplicate(const std::vector<ClipId>& clips) const {
  assert(history_ != nullptr);
  assert(clips.size() < 1000000);
  std::vector<ClipId> made;
  const auto placed = WithDuplicates(history_->current(), clips, &made);
  return placed ? QString() : placed.error().message;
}

Result<std::vector<ClipId>> ReelManager::DuplicateAll(
    const std::vector<ClipId>& clips) {
  assert(history_ != nullptr);
  assert(clips.size() < 1000000);
  std::vector<ClipId> made;
  auto next = WithDuplicates(history_->current(), clips, &made);
  const size_t count = made.size();
  auto applied = history_->Apply(
      CountLabel(count, Tr("Duplicate clip"), Tr("Duplicate clips")),
      std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return made;
}

}  // namespace snapper
