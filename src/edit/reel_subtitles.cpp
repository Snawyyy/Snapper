// ReelManager's subtitle track: tapping cues in time, rolling a cue's
// edge, the track's look, and SRT and WebVTT files.

#include <QFile>
#include <QStringConverter>
#include <QTextStream>

#include <algorithm>
#include <cassert>
#include <utility>
#include <variant>

#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/reel_edits.h"
#include "edit/reel_manager.h"

namespace snapper {
namespace {

// The subtitle track's index, made on top when the reel has none.
int MakeSubtitleTrack(Reel* reel) {
  assert(reel != nullptr);
  assert(reel->tracks.size() < static_cast<size_t>(kMaxAllTracks) ||
         SubtitleTrack(*reel) >= 0);
  const int found = SubtitleTrack(*reel);
  if (found >= 0) {
    return found;
  }
  ReelTrack track;
  track.is_subtitle = true;
  reel->tracks.push_back(std::move(track));
  return static_cast<int>(reel->tracks.size()) - 1;
}

// A cue showing line.
TextSource CueText(const QString& line) {
  assert(line.size() <= kMaxReelTextLength * 2);
  TextSource text;
  text.text = line.left(kMaxReelTextLength);
  return text;
}

// Takes away whatever on track overlaps from to to, shortening the
// clips that cross from.
void ClearSpan(ReelTrack* track, Frame from, Frame to) {
  assert(track != nullptr);
  assert(from < to);
  for (Clip& clip : track->clips) {
    const bool is_crossing = clip.start < from && from < clip.end();
    if (is_crossing) {
      clip.length = Frame(from.index() - clip.start.index());
    }
  }
  std::erase_if(track->clips, [from, to](const Clip& clip) {
    return clip.start >= from && clip.start < to;
  });
}

Result<QString> ReadText(const QString& path) {
  assert(!path.isEmpty());
  assert(path.size() < 100000);
  QFile file(path);
  const bool is_open = file.open(QIODevice::ReadOnly);
  if (!is_open) {
    return std::unexpected(
        Error{Tr("Can't read %1: %2").arg(path, file.errorString())});
  }
  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  return stream.readAll();
}

}  // namespace

Result<void> ReelManager::SetSubtitleLook(const ReelTextLook& look,
                                          const QString& label) {
  assert(history_ != nullptr);
  assert(!label.isEmpty());
  const bool is_valid = look.letters.size >= 1.0 &&
                        !look.letters.font_family.isEmpty();
  if (!is_valid) {
    return std::unexpected(Error{Tr("Text needs a font and a size.")});
  }
  Project next = history_->current();
  next.reel.subtitle_look = look;
  return history_->Apply(label, std::move(next));
}

Result<void> ReelManager::SetSubtitleLines(const QString& text) {
  assert(history_ != nullptr);
  assert(text.size() >= 0);
  std::vector<QString> lines;
  for (const QString& line : text.split(QLatin1Char('\n'))) {
    const QString kept = line.trimmed();
    const bool is_kept = !kept.isEmpty() &&
                         lines.size() < static_cast<size_t>(kMaxSubtitleLines);
    if (is_kept) {
      lines.push_back(kept);
    }
  }
  Project next = history_->current();
  next.reel.subtitle_lines = std::move(lines);
  return history_->Apply(Tr("Paste lines"), std::move(next));
}

QString ReelManager::WhyNoTap(Frame at) const {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const Reel& reel = history_->current().reel;
  const bool has_lines = !reel.subtitle_lines.empty();
  if (!has_lines) {
    return Tr("Paste the lines to time into the Subtitles box first.");
  }
  const int track = SubtitleTrack(reel);
  const bool is_taken =
      track >= 0 &&
      std::ranges::any_of(reel.tracks[static_cast<size_t>(track)].clips,
                          [at](const Clip& clip) { return clip.start == at; });
  return is_taken ? Tr("A cue already starts here.") : QString();
}

Result<ClipId> ReelManager::TapCue(Frame at) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const QString why_not = WhyNoTap(at);
  if (!why_not.isEmpty()) {
    return std::unexpected(Error{why_not});
  }
  Project next = history_->current();
  const int track = MakeSubtitleTrack(&next.reel);
  int room = kCueFrames;
  for (Clip& clip : TrackOf(&next, track).clips) {
    // The cue running now ends here.
    const bool is_running = clip.start < at && at < clip.end();
    if (is_running) {
      clip.length = Frame(at.index() - clip.start.index());
    }
    const bool is_later = clip.start > at;
    if (is_later) {
      room = std::min(room, clip.start.index() - at.index());
    }
  }
  const QString line = next.reel.subtitle_lines.front();
  next.reel.subtitle_lines.erase(next.reel.subtitle_lines.begin());
  ClipId id;
  auto applied = history_->Apply(
      Tr("Time cue"), WithNewClip(std::move(next), CueText(line), track, at,
                                  Frame(room), &id));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  last_cue_ = id;
  return id;
}

Result<void> ReelManager::ReleaseCue(Frame at, Frame held) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const bool is_hold = held.index() >= kHoldFrames && last_cue_.IsValid();
  if (!is_hold) {
    return {};
  }
  const Clip* cue = ClipOf(history_->current().reel, last_cue_);
  const bool is_inside = cue != nullptr && cue->start < at && at < cue->end();
  if (!is_inside) {
    return {};
  }
  return TrimEnd(last_cue_, at);
}

Result<void> ReelManager::RollCueEdge(ClipId clip, Frame end) {
  assert(history_ != nullptr);
  assert(end.index() >= 0);
  Project next = history_->before();
  const ClipSpot spot = FindClip(next.reel, clip);
  const bool is_present = spot.IsValid();
  if (!is_present) {
    return std::unexpected(Error{Tr("That clip no longer exists.")});
  }
  ReelTrack& track = TrackOf(&next, spot.track);
  Clip& cue = track.clips[static_cast<size_t>(spot.index)];
  const Clip* touching = NextTouching(track, cue);
  const bool has_next = touching != nullptr;
  if (!has_next) {
    return TrimEnd(clip, end);
  }
  Clip& after = track.clips[static_cast<size_t>(spot.index) + 1];
  assert(after.id == touching->id);
  const bool is_inside = cue.start < end && end < after.end();
  if (!is_inside) {
    return std::unexpected(Error{Tr("Both cues keep a frame at least.")});
  }
  const Frame after_end = after.end();
  cue.length = Frame(end.index() - cue.start.index());
  after.start = end;
  after.length = Frame(after_end.index() - end.index());
  return history_->Apply(Tr("Move cue"), std::move(next));
}

Result<void> ReelManager::RollCueStart(ClipId clip, Frame start) {
  assert(history_ != nullptr);
  assert(start.index() >= 0);
  const Reel& reel = history_->before().reel;
  const ClipSpot spot = FindClip(reel, clip);
  const bool is_present = spot.IsValid();
  if (!is_present) {
    return std::unexpected(Error{Tr("That clip no longer exists.")});
  }
  const ReelTrack& track = reel.tracks[static_cast<size_t>(spot.track)];
  const Clip& cue = track.clips[static_cast<size_t>(spot.index)];
  const bool has_before =
      spot.index > 0 &&
      track.clips[static_cast<size_t>(spot.index) - 1].end() == cue.start;
  if (!has_before) {
    return TrimStart(clip, start);
  }
  return RollCueEdge(track.clips[static_cast<size_t>(spot.index) - 1].id,
                     start);
}

bool ReelManager::IsCueRoll(ClipId clip, bool is_alt) const {
  assert(history_ != nullptr);
  assert(clip.value() >= 0);
  const Reel& reel = history_->current().reel;
  const int subtitles = SubtitleTrack(reel);
  return is_alt && subtitles >= 0 && FindClip(reel, clip).track == subtitles;
}

Result<std::vector<ClipId>> ReelManager::ImportSubtitles(const QString& path) {
  assert(history_ != nullptr);
  assert(!path.isEmpty());
  const auto content = ReadText(path);
  if (!content) {
    return std::unexpected(content.error());
  }
  const auto cues = ParseSubtitles(*content);
  if (!cues) {
    return std::unexpected(cues.error());
  }
  Project next = history_->current();
  const int track = MakeSubtitleTrack(&next.reel);
  std::vector<ClipId> made;
  Frame free_from;
  for (const SubtitleCue& cue : *cues) {
    // A cue starting inside the one before starts where that one ends.
    const Frame start = std::max(cue.start, free_from);
    const bool has_length = start < cue.end;
    const bool has_room =
        TrackOf(&next, track).clips.size() <
        static_cast<size_t>(kMaxClipsPerTrack);
    if (!has_length || !has_room) {
      continue;
    }
    ClearSpan(&TrackOf(&next, track), start, cue.end);
    ClipId id;
    auto placed = WithNewClip(std::move(next), CueText(cue.text), track,
                              start, Frame(cue.end.index() - start.index()),
                              &id);
    if (!placed) {
      return std::unexpected(placed.error());
    }
    next = std::move(*placed);
    made.push_back(id);
    free_from = cue.end;
  }
  auto applied = history_->Apply(Tr("Import subtitles"), std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return made;
}

Result<int> ReelManager::ReplaceInCues(const QString& find,
                                       const QString& with) {
  assert(history_ != nullptr);
  assert(with.size() < 100000);
  if (find.isEmpty()) {
    return std::unexpected(Error{Tr("Type what to find first.")});
  }
  Project next = history_->current();
  const int track = SubtitleTrack(next.reel);
  int changed = 0;
  if (track >= 0) {
    for (Clip& clip : TrackOf(&next, track).clips) {
      auto& text = std::get<TextSource>(clip.source);
      const bool has_it = text.text.contains(find);
      if (has_it) {
        text.text.replace(find, with);
        text.text = text.text.left(kMaxReelTextLength);
        ++changed;
      }
    }
  }
  if (changed == 0) {
    return std::unexpected(Error{Tr("No cue holds \"%1\".").arg(find)});
  }
  auto applied = history_->Apply(Tr("Replace in cues"), std::move(next));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return changed;
}

std::vector<SubtitleCue> ReelManager::CuesOf(const Reel& reel) {
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
  assert(kMaxClipsPerTrack > 0);
  std::vector<SubtitleCue> cues;
  const int track = SubtitleTrack(reel);
  if (track < 0) {
    return cues;
  }
  for (const Clip& clip : reel.tracks[static_cast<size_t>(track)].clips) {
    const auto* text = std::get_if<TextSource>(&clip.source);
    if (text != nullptr) {
      cues.push_back({clip.start, clip.end(), text->text});
    }
  }
  return cues;
}

}  // namespace snapper
