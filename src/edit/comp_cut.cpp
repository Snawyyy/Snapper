// CompManager: the Video tab's clip edits on effect clips: split at the
// playhead, cut, paste at the playhead and duplicate, each one undo
// step.

#include <algorithm>
#include <cassert>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "anim/comp_reel.h"
#include "anim/reel_timeline.h"
#include "base/text.h"
#include "edit/comp_manager.h"
#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "model/project.h"

namespace snapper {

std::optional<CompManager::ClipFrames> CompManager::FramesOf(
    const CompClipRef& clip) const {
  assert(history_ != nullptr);
  assert(clip.id.value() >= 0);
  const CompEffect* effect = Find(clip.shot, clip.id);
  const Clip* on_reel = FirstClipOf(history_->current(), clip.shot);
  const bool is_there = effect != nullptr && on_reel != nullptr;
  if (!is_there) {
    return std::nullopt;
  }
  const ReelSpan span = EffectOnReel(*on_reel, *effect);
  const int offset = on_reel->start.index() - on_reel->in.index();
  return ClipFrames{CompSpan{span.first - offset, span.end - offset},
                    span.first};
}

QString CompManager::WhyNoPasteAt(Frame reel) const {
  assert(history_ != nullptr);
  assert(reel.index() >= 0);
  const bool has_copy = !copied_.empty();
  if (!has_copy) {
    return Tr("Copy an effect first (Ctrl+C).");
  }
  const ReelShot at = ShotAtReel(history_->current(), reel);
  const bool is_on_shot = at.IsValid();
  if (!is_on_shot) {
    return Tr("Move the playhead over a shot to paste there.");
  }
  return WhyNoPaste(at.shot);
}

Result<void> CompManager::PasteAt(Frame reel, const CompTarget& elsewhere) {
  assert(history_ != nullptr);
  assert(elsewhere.layer.value() >= 0);
  const QString why = WhyNoPasteAt(reel);
  const bool can_paste = why.isEmpty();
  if (!can_paste) {
    return std::unexpected(Error{why});
  }
  const Project& project = history_->current();
  const ShotId shot = ShotAtReel(project, reel).shot;
  const Clip* on_reel = FirstClipOf(project, shot);
  const Shot* found = FindShot(project, shot);
  assert(on_reel != nullptr && found != nullptr);
  const int offset = on_reel->start.index() - on_reel->in.index();
  const int shot_length = found->length.index();
  const std::vector<Copied> copies = copied_;
  int earliest = reel.index();
  for (const Copied& copy : copies) {
    earliest = copy.frames ? std::min(earliest, copy.frames->reel_first)
                           : earliest;
  }
  EditScope scope(history_, Tr("Paste %1").arg(CopiedName()));
  std::set<CompClipRef> made;
  CompClipRef last{};
  for (const Copied& copy : copies) {
    CompEffect effect = copy.effect;
    const ClipFrames frames = copy.frames.value_or(
        ClipFrames{SpanOf(effect, shot_length), earliest});
    const bool is_other_shot = copy.shot != shot;
    if (is_other_shot) {
      effect.target.layer = elsewhere.layer;
    }
    effect.start = frames.local.first;
    effect.length = std::max(1, frames.local.end - frames.local.first);
    const int length = effect.length;
    const int start = reel.index() + frames.reel_first - earliest - offset;
    const auto id = Insert(shot, std::move(effect), kOnTop,
                           Tr("Paste effect"));
    if (!id) {
      return std::unexpected(id.error());
    }
    const auto timed = SetTiming(shot, *id, start, length, true);
    if (!timed) {
      return timed;
    }
    last = CompClipRef{shot, *id};
    made.insert(last);
  }
  PickClips(made, PickMode::kReplace, last);
  return {};
}

QString CompManager::WhyNoSplit(const std::set<CompClipRef>& clips,
                                Frame reel) const {
  assert(history_ != nullptr);
  assert(reel.index() >= 0);
  const bool is_none = clips.empty();
  if (is_none) {
    return Tr("Pick an effect clip to split it.");
  }
  bool is_any_inside = false;
  for (const CompClipRef& clip : clips) {
    const std::optional<ClipFrames> frames = FramesOf(clip);
    const int last = frames ? frames->reel_first + frames->local.end -
                                  frames->local.first
                            : 0;
    const bool is_inside = frames && reel.index() > frames->reel_first &&
                           reel.index() < last;
    const QString why_not = is_inside ? WhyNoAdd(clip.shot) : QString();
    const bool is_blocked = !why_not.isEmpty();
    if (is_blocked) {
      return why_not;
    }
    is_any_inside = is_any_inside || is_inside;
  }
  return is_any_inside
             ? QString()
             : Tr("Move the playhead inside a picked clip to split it.");
}

Result<void> CompManager::SplitAll(const std::set<CompClipRef>& clips,
                                   Frame reel) {
  assert(history_ != nullptr);
  assert(reel.index() >= 0);
  const QString why = WhyNoSplit(clips, reel);
  const bool can_split = why.isEmpty();
  if (!can_split) {
    return std::unexpected(Error{why});
  }
  EditScope scope(history_, Tr("Split effects"));
  std::set<CompClipRef> rights;
  CompClipRef last{};
  for (const CompClipRef& clip : clips) {
    const std::optional<ClipFrames> frames = FramesOf(clip);
    const int cut = frames ? frames->local.first + reel.index() -
                                 frames->reel_first
                           : 0;
    const bool is_inside =
        frames && cut > frames->local.first && cut < frames->local.end;
    if (!is_inside) {
      continue;
    }
    const auto left = SetTiming(clip.shot, clip.id, frames->local.first,
                                cut - frames->local.first, false);
    if (!left) {
      return left;
    }
    CompEffect right = *Find(clip.shot, clip.id);
    right.start = cut;
    right.length = frames->local.end - cut;
    const int index = CompEffectIndex(Of(clip.shot), clip.id);
    const auto made = Insert(clip.shot, std::move(right), index + 1,
                             Tr("Split effect"));
    if (!made) {
      return std::unexpected(made.error());
    }
    last = CompClipRef{clip.shot, *made};
    rights.insert(last);
  }
  PickClips(rights, PickMode::kReplace, last);
  return {};
}

Result<void> CompManager::CutAll(const std::set<CompClipRef>& clips) {
  assert(history_ != nullptr);
  assert(clips.size() < 100000);
  const auto copied = CopyClips(clips);
  if (!copied) {
    return copied;
  }
  EditScope scope(history_, Tr("Cut %1").arg(CopiedName()));
  for (const CompClipRef& clip : clips) {
    const auto removed = Remove(clip.shot, clip.id);
    if (!removed) {
      return removed;
    }
  }
  return {};
}

Result<void> CompManager::DuplicateAll(const std::set<CompClipRef>& clips) {
  assert(history_ != nullptr);
  assert(clips.size() < 100000);
  const bool is_none = clips.empty();
  if (is_none) {
    return std::unexpected(Error{Tr("Pick an effect clip to duplicate it.")});
  }
  EditScope scope(history_, clips.size() == 1 ? Tr("Duplicate effect")
                                              : Tr("Duplicate effects"));
  std::set<CompClipRef> made;
  CompClipRef last{};
  for (const CompClipRef& clip : clips) {
    const auto copy = Duplicate(clip.shot, clip.id);
    if (!copy) {
      return std::unexpected(copy.error());
    }
    last = CompClipRef{clip.shot, *copy};
    made.insert(last);
  }
  PickClips(made, PickMode::kReplace, last);
  return {};
}

}  // namespace snapper
