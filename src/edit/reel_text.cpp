// ReelManager's text clips: adding them, their words, look, place,
// motions and named styles.

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <set>
#include <utility>
#include <variant>

#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/reel_edits.h"
#include "edit/reel_manager.h"
#include "render/layer_painter.h"
#include "render/reel_text.h"

namespace snapper {
namespace {

Error NotText() { return Error{Tr("Pick a text clip first.")}; }

// The highest picture track with room for a text clip at at, or -1.
int FreeTextTrack(const Reel& reel, Frame at) {
  assert(at.index() >= 0);
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
  Clip probe;
  probe.start = at;
  probe.length = Frame(kTextClipFrames);
  for (int t = static_cast<int>(reel.tracks.size()) - 1; t >= 0; --t) {
    const ReelTrack& track = reel.tracks[static_cast<size_t>(t)];
    const bool is_free =
        !track.is_sound && !track.is_subtitle && HasRoom(track, probe);
    if (is_free) {
      return t;
    }
  }
  return -1;
}

// Calls change(text, look) on the text clip id inside project, look
// being the one it shows: its own, or the subtitle track's for a cue
// sharing it.
template <typename Change>
Result<void> EditText(Project* project, ClipId id, Change change) {
  assert(project != nullptr);
  assert(id.value() >= 0);
  const ClipSpot spot = FindClip(project->reel, id);
  const bool is_present = spot.IsValid();
  if (!is_present) {
    return std::unexpected(Error{Tr("That clip no longer exists.")});
  }
  Clip& clip =
      TrackOf(project, spot.track).clips[static_cast<size_t>(spot.index)];
  auto* text = std::get_if<TextSource>(&clip.source);
  if (text == nullptr) {
    return std::unexpected(NotText());
  }
  const bool is_shared = &LookOf(project->reel, clip) ==
                         &project->reel.subtitle_look;
  change(text, is_shared ? &project->reel.subtitle_look : &text->look,
         is_shared);
  return {};
}

// The same for every clip in ids that is text; refused when none is.
template <typename Change>
Result<Project> EditTexts(Project project, const std::vector<ClipId>& ids,
                          Change change) {
  assert(ids.size() < 1000000);
  assert(project.reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
  int edited = 0;
  for (const ClipId id : std::set<ClipId>(ids.begin(), ids.end())) {
    const Clip* clip = ClipOf(project.reel, id);
    const bool is_text = clip != nullptr && IsTextClip(*clip);
    if (is_text) {
      const auto done = EditText(&project, id, change);
      edited += done ? 1 : 0;
    }
  }
  if (edited == 0) {
    return std::unexpected(NotText());
  }
  return project;
}

// look's lettering box on a canvas, middle at place, in shares of the
// frame.
QRectF ShareRect(const Project& project, const ReelTextLook& look,
                 const QString& text) {
  assert(IsValidCanvas(project.canvas));
  assert(look.letters.size > 0.0);
  const double width = project.canvas.width;
  const double height = project.canvas.height;
  const QRectF box = TextBox(ReelLettering(look, text, width));
  return QRectF(look.place.x() + box.left() / width,
                look.place.y() + box.top() / height, box.width() / width,
                box.height() / height);
}

// The nearest pull within reach of value's guides: middle, low edge and
// high edge of a span from low to high around middle.
double Pull(double middle, double low, double high, double reach,
            bool* found) {
  assert(found != nullptr);
  assert(reach >= 0.0);
  const std::array<double, 3> pulls = {0.5 - middle, kTitleSafe - low,
                                       1.0 - kTitleSafe - high};
  double best = 0.0;
  *found = false;
  for (const double pull : pulls) {
    const bool is_nearer =
        std::abs(pull) <= reach && (!*found || std::abs(pull) < std::abs(best));
    if (is_nearer) {
      best = pull;
      *found = true;
    }
  }
  return best;
}

QPointF SpotPlace(TextSpot spot, const QRectF& rect) {
  assert(static_cast<int>(spot) < kTextSpotCount);
  assert(rect.width() >= 0.0);
  const double half_w = rect.width() / 2.0;
  const double half_h = rect.height() / 2.0;
  switch (spot) {
    case TextSpot::kBottom:
      return QPointF(0.5, 1.0 - kTitleSafe - half_h);
    case TextSpot::kTop:
      return QPointF(0.5, kTitleSafe + half_h);
    case TextSpot::kLowerThird:
      return QPointF(kTitleSafe + half_w, 2.0 / 3.0 + half_h);
    case TextSpot::kMiddle:
      break;
  }
  return QPointF(0.5, 0.5);
}

// look takes from's lettering and box, keeping its own place.
void TakeLook(const ReelTextLook& from, ReelTextLook* look) {
  assert(look != nullptr);
  assert(from.letters.size > 0.0);
  const QPointF place = look->place;
  *look = from;
  look->place = place;
}

}  // namespace

QString ReelManager::WhyNoAddText(Frame at) const {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const bool has_room = FreeTextTrack(history_->current().reel, at) >= 0;
  const bool can_add_track = WhyNoAddTrack().isEmpty();
  return has_room || can_add_track
             ? QString()
             : Tr("Every track has a clip at the playhead.");
}

Result<ClipId> ReelManager::AddText(Frame at) {
  assert(history_ != nullptr);
  assert(at.index() >= 0);
  const QString why_not = WhyNoAddText(at);
  if (!why_not.isEmpty()) {
    return std::unexpected(Error{why_not});
  }
  Project next = history_->current();
  int track = FreeTextTrack(next.reel, at);
  const bool needs_track = track < 0;
  if (needs_track) {
    // A new picture track on top, under the subtitle track.
    const int subtitles = SubtitleTrack(next.reel);
    track = subtitles >= 0 ? subtitles
                           : static_cast<int>(next.reel.tracks.size());
    next.reel.tracks.insert(next.reel.tracks.begin() + track, ReelTrack());
  }
  TextSource text;
  text.text = Tr("Your words");
  ClipId id;
  auto applied = history_->Apply(
      Tr("Add text"), WithNewClip(std::move(next), std::move(text), track,
                                  at, Frame(kTextClipFrames), &id));
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return id;
}

Result<void> ReelManager::SetText(ClipId clip, const QString& text) {
  assert(history_ != nullptr);
  assert(clip.value() >= 0);
  const QString kept = text.left(kMaxReelTextLength);
  Project next = history_->current();
  auto done = EditText(&next, clip,
                       [&kept](TextSource* edited, ReelTextLook*, bool) {
                         edited->text = kept;
                       });
  if (!done) {
    return done;
  }
  return history_->Apply(Tr("Edit text"), std::move(next));
}

Result<void> ReelManager::SetTextLook(const std::vector<ClipId>& clips,
                                      const ReelTextLook& look,
                                      const QString& label) {
  assert(history_ != nullptr);
  assert(!label.isEmpty());
  const bool is_valid = look.letters.size >= 1.0 &&
                        look.letters.outline_width >= 0.0 &&
                        !look.letters.font_family.isEmpty();
  if (!is_valid) {
    return std::unexpected(Error{Tr("Text needs a font and a size.")});
  }
  return history_->Apply(
      label, EditTexts(history_->current(), clips,
                       [&look](TextSource* text, ReelTextLook* shown,
                               bool is_shared) {
                         TakeLook(look, shown);
                         if (!is_shared) {
                           text->style.clear();
                         }
                       }));
}

Result<TextPlaced> ReelManager::PlaceText(ClipId clip, QPointF place,
                                          double size, double reach) {
  assert(history_ != nullptr);
  assert(reach >= 0.0);
  Project next = history_->before();
  const Clip* found = ClipOf(next.reel, clip);
  const bool is_text = found != nullptr && IsTextClip(*found);
  if (!is_text) {
    return std::unexpected(NotText());
  }
  ReelTextLook look = LookOf(next.reel, *found);
  look.letters.size = std::clamp(size, 1.0, 2000.0);
  look.place = place;
  const QRectF rect =
      ShareRect(next, look, std::get<TextSource>(found->source).text);
  TextPlaced placed;
  const double dx = Pull(look.place.x(), rect.left(), rect.right(), reach,
                         &placed.is_snapped_x);
  const double dy = Pull(look.place.y(), rect.top(), rect.bottom(), reach,
                         &placed.is_snapped_y);
  look.place += QPointF(dx, dy);
  placed.place = look.place;
  auto done = EditText(&next, clip,
                       [&look](TextSource*, ReelTextLook* shown, bool) {
                         shown->letters.size = look.letters.size;
                         shown->place = look.place;
                       });
  auto applied = done ? history_->Apply(Tr("Move text"), std::move(next))
                      : done;
  if (!applied) {
    return std::unexpected(applied.error());
  }
  return placed;
}

Result<void> ReelManager::SetTextSpot(const std::vector<ClipId>& clips,
                                      TextSpot spot) {
  assert(history_ != nullptr);
  assert(static_cast<int>(spot) < kTextSpotCount);
  const Project& now = history_->current();
  return history_->Apply(
      Tr("Place text"),
      EditTexts(now, clips,
                [&now, spot](TextSource* text, ReelTextLook* shown, bool) {
                  shown->place = SpotPlace(spot,
                                           ShareRect(now, *shown, text->text));
                }));
}

Result<void> ReelManager::SetTextMotion(const std::vector<ClipId>& clips,
                                        bool is_appear, TextMotion motion,
                                        Frame length) {
  assert(history_ != nullptr);
  assert(static_cast<int>(motion) < kTextMotionCount);
  const Frame kept(std::clamp(length.index(), 0, kCueFrames * 4));
  return history_->Apply(
      is_appear ? Tr("Text in") : Tr("Text out"),
      EditTexts(history_->current(), clips,
                [&](TextSource* text, ReelTextLook*, bool) {
                  (is_appear ? text->appear : text->leave) = motion;
                  (is_appear ? text->appear_length : text->leave_length) =
                      kept;
                }));
}

Result<void> ReelManager::SaveTextStyle(const QString& name, ClipId from) {
  assert(history_ != nullptr);
  assert(from.value() >= 0);
  const QString kept = name.trimmed();
  Project next = history_->current();
  const Clip* clip = ClipOf(next.reel, from);
  const bool is_text = clip != nullptr && IsTextClip(*clip);
  if (!is_text || kept.isEmpty()) {
    return std::unexpected(
        Error{Tr("Pick a text clip and give the style a name.")});
  }
  const ReelTextLook look = LookOf(next.reel, *clip);
  auto& styles = next.reel.text_styles;
  const auto same = std::ranges::find_if(
      styles, [&kept](const TextStyle& style) { return style.name == kept; });
  const bool is_new = same == styles.end();
  const bool is_full = is_new && styles.size() >=
                                     static_cast<size_t>(kMaxTextStyles);
  if (is_full) {
    return std::unexpected(
        Error{Tr("A project holds at most %1 styles.").arg(kMaxTextStyles)});
  }
  if (is_new) {
    styles.push_back({kept, look});
  } else {
    same->look = look;
  }
  (void)EditText(&next, from, [&kept](TextSource* text, ReelTextLook*,
                                      bool is_shared) {
    text->style = is_shared ? QString() : kept;
  });
  // Every clip following the style takes its new look.
  for (ReelTrack& track : next.reel.tracks) {
    for (Clip& other : track.clips) {
      auto* text = std::get_if<TextSource>(&other.source);
      const bool is_linked = text != nullptr && text->style == kept;
      if (is_linked) {
        TakeLook(look, &text->look);
      }
    }
  }
  return history_->Apply(Tr("Save style %1").arg(kept), std::move(next));
}

Result<void> ReelManager::ApplyTextStyle(const std::vector<ClipId>& clips,
                                         const QString& name,
                                         bool is_linked) {
  assert(history_ != nullptr);
  assert(clips.size() < 1000000);
  const TextStyle* style = FindTextStyle(history_->current().reel, name);
  if (style == nullptr) {
    return std::unexpected(Error{Tr("That style no longer exists.")});
  }
  const ReelTextLook look = style->look;
  return history_->Apply(
      Tr("Apply style %1").arg(name),
      EditTexts(history_->current(), clips,
                [&](TextSource* text, ReelTextLook* shown, bool is_shared) {
                  TakeLook(look, shown);
                  if (!is_shared) {
                    text->style = is_linked ? name : QString();
                  }
                }));
}

Result<void> ReelManager::RemoveTextStyle(const QString& name) {
  assert(history_ != nullptr);
  assert(name.size() < 100000);
  Project next = history_->current();
  const auto gone = std::erase_if(
      next.reel.text_styles,
      [&name](const TextStyle& style) { return style.name == name; });
  if (gone == 0) {
    return std::unexpected(Error{Tr("That style no longer exists.")});
  }
  for (ReelTrack& track : next.reel.tracks) {
    for (Clip& clip : track.clips) {
      auto* text = std::get_if<TextSource>(&clip.source);
      const bool is_linked = text != nullptr && text->style == name;
      if (is_linked) {
        text->style.clear();
      }
    }
  }
  return history_->Apply(Tr("Remove style %1").arg(name), std::move(next));
}

Result<void> ReelManager::SetOwnLook(const std::vector<ClipId>& clips,
                                     bool is_own) {
  assert(history_ != nullptr);
  assert(clips.size() < 1000000);
  Project next = history_->current();
  const ReelTextLook shared = next.reel.subtitle_look;
  const int subtitles = SubtitleTrack(next.reel);
  int changed = 0;
  for (const ClipId id : clips) {
    const ClipSpot spot = FindClip(next.reel, id);
    const bool is_cue = spot.IsValid() && spot.track == subtitles;
    if (!is_cue) {
      continue;
    }
    auto& text = std::get<TextSource>(
        TrackOf(&next, spot.track).clips[static_cast<size_t>(spot.index)]
            .source);
    const bool is_starting = is_own && !text.is_own_look;
    if (is_starting) {
      // Its own look starts as the one it showed.
      text.look = shared;
    }
    text.is_own_look = is_own;
    ++changed;
  }
  if (changed == 0) {
    return std::unexpected(Error{Tr("Pick a cue on the subtitle track.")});
  }
  return history_->Apply(is_own ? Tr("Cue keeps its own look")
                                : Tr("Cue shares the track's look"),
                         std::move(next));
}

}  // namespace snapper
