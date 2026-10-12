#include "io/reel_json.h"

#include <QJsonObject>

#include <algorithm>
#include <array>
#include <cassert>
#include <variant>

#include "base/text.h"
#include "io/layer_json.h"
#include "model/project.h"

namespace snapper {
namespace {

constexpr std::array<const char*, kTransitionKindCount> kTransitionNames = {
    "cut",        "swipe_left", "swipe_right", "swipe_up",
    "swipe_down", "flash",      "crossfade"};

QJsonObject TransitionToJson(const Transition& transition) {
  assert(static_cast<int>(transition.kind) < kTransitionKindCount);
  assert(transition.length.index() >= 0);
  return QJsonObject{
      {"kind",
       EnumToJson(static_cast<int>(transition.kind), kTransitionNames)},
      {"length", transition.length.index()}};
}

Transition TransitionFromJson(const QJsonValue& value, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(kTransitionNames.size() == kTransitionKindCount);
  const QJsonObject object = value.toObject();
  Transition transition;
  transition.kind = static_cast<TransitionKind>(
      EnumFromJson(object.value("kind"), kTransitionNames, issues));
  transition.length = Frame(std::max(object.value("length").toInt(), 0));
  return transition;
}

QJsonObject LookToJson(const ReelTextLook& look) {
  assert(look.letters.size > 0.0);
  assert(look.box.isValid());
  QJsonObject object;
  AddLetters(look.letters, &object);
  object.insert("box", ColorToJson(look.box));
  object.insert("place", PointToJson(look.place));
  return object;
}

ReelTextLook LookFromJson(const QJsonValue& value, ReelTextLook fallback,
                          JsonIssues* issues) {
  assert(issues != nullptr);
  assert(fallback.box.isValid());
  const bool is_missing = !value.isObject();
  if (is_missing) {
    return fallback;
  }
  const QJsonObject object = value.toObject();
  ReelTextLook look = fallback;
  ReadLetters(object, &look.letters, issues);
  look.box = ColorFromJson(object.value("box"), look.box, issues);
  const bool has_place = object.contains("place");
  if (has_place) {
    const QPointF place = PointFromJson(object.value("place"));
    look.place = QPointF(std::clamp(place.x(), -1.0, 2.0),
                         std::clamp(place.y(), -1.0, 2.0));
  }
  return look;
}

constexpr std::array<const char*, kTextMotionCount> kMotionNames = {
    "cut", "fade", "pop", "slide_up", "type_on"};

QJsonObject TextToJson(const TextSource& text) {
  assert(static_cast<int>(text.appear) < kTextMotionCount);
  assert(static_cast<int>(text.leave) < kTextMotionCount);
  QJsonObject object{
      {"words", text.text},
      {"look", LookToJson(text.look)},
      {"appear", EnumToJson(static_cast<int>(text.appear), kMotionNames)},
      {"appear_length", text.appear_length.index()},
      {"leave", EnumToJson(static_cast<int>(text.leave), kMotionNames)},
      {"leave_length", text.leave_length.index()}};
  if (!text.style.isEmpty()) {
    object.insert("style", text.style);
  }
  if (text.is_own_look) {
    object.insert("own_look", true);
  }
  return object;
}

TextSource TextFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(kMotionNames.size() == kTextMotionCount);
  TextSource text;
  text.text = object.value("words").toString().left(kMaxReelTextLength);
  text.look = LookFromJson(object.value("look"), ReelTextLook(), issues);
  text.appear = static_cast<TextMotion>(
      EnumFromJson(object.value("appear"), kMotionNames, issues));
  text.appear_length = Frame(object.value("appear_length").toInt(6));
  text.leave = static_cast<TextMotion>(
      EnumFromJson(object.value("leave"), kMotionNames, issues));
  text.leave_length = Frame(object.value("leave_length").toInt(6));
  text.style = object.value("style").toString();
  text.is_own_look = object.value("own_look").toBool();
  return text;
}

QJsonObject ClipToJson(const Clip& clip) {
  assert(clip.id.IsValid());
  assert(clip.length.index() >= 1);
  QJsonObject object{{"id", clip.id.value()},
                     {"start", clip.start.index()},
                     {"in", clip.in.index()},
                     {"length", clip.length.index()}};
  const auto* video = std::get_if<VideoSource>(&clip.source);
  const auto* sound = std::get_if<SoundSource>(&clip.source);
  const auto* shot = std::get_if<ShotSource>(&clip.source);
  const auto* text = std::get_if<TextSource>(&clip.source);
  if (text != nullptr) {
    object.insert("text", TextToJson(*text));
  } else if (video != nullptr) {
    object.insert("video", video->path);
    object.insert("video_length", video->length.index());
  } else if (sound != nullptr) {
    object.insert("sound", sound->path);
    object.insert("sound_length", sound->length.index());
    object.insert("volume", clip.volume);
    object.insert("fade_in", clip.fade_in.index());
    object.insert("fade_out", clip.fade_out.index());
  } else {
    assert(shot != nullptr);
    object.insert("shot", shot->shot.value());
  }
  const bool has_transition = clip.out.kind != TransitionKind::kCut;
  if (has_transition) {
    object.insert("transition", TransitionToJson(clip.out));
  }
  return object;
}

Clip ClipFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(object.size() >= 0);
  Clip clip;
  clip.id = ClipId(object.value("id").toInt());
  clip.start = Frame(object.value("start").toInt());
  clip.in = Frame(object.value("in").toInt());
  clip.length = Frame(object.value("length").toInt());
  const bool is_video = object.contains("video");
  const bool is_sound = object.contains("sound");
  const bool is_text = object.value("text").isObject();
  if (is_text) {
    clip.source = TextFromJson(object.value("text").toObject(), issues);
  } else if (is_video) {
    clip.source = VideoSource{object.value("video").toString(),
                              Frame(object.value("video_length").toInt())};
  } else if (is_sound) {
    clip.source = SoundSource{object.value("sound").toString(),
                              Frame(object.value("sound_length").toInt())};
    clip.volume =
        std::clamp(object.value("volume").toDouble(1.0), 0.0, kMaxGain);
    clip.fade_in = Frame(std::max(object.value("fade_in").toInt(), 0));
    clip.fade_out = Frame(std::max(object.value("fade_out").toInt(), 0));
  } else {
    clip.source = ShotSource{ShotId(object.value("shot").toInt())};
  }
  const bool has_transition = object.contains("transition");
  if (has_transition) {
    clip.out = TransitionFromJson(object.value("transition"), issues);
  }
  const bool is_valid = clip.id.IsValid() && clip.length.index() >= 1;
  if (!is_valid) {
    issues->Note(Tr("a clip has no id or no length"));
  }
  return clip;
}

ReelTrack TrackFromJson(const QJsonValue& value, JsonIssues* issues) {
  assert(issues != nullptr);
  // A picture track is a bare list of clips; a sound or subtitle track
  // an object with its header settings and its clips.
  const QJsonObject header = value.toObject();
  const bool is_sound = value.isObject() && header.value("sound").toBool();
  const bool is_subtitle =
      value.isObject() && header.value("subtitles").toBool();
  const QJsonArray clips = value.isObject() ? header.value("clips").toArray()
                                            : value.toArray();
  ReelTrack track;
  track.is_sound = is_sound;
  track.is_subtitle = is_subtitle && !is_sound;
  if (is_sound) {
    track.name = header.value("name").toString();
    track.is_muted = header.value("muted").toBool();
    track.is_solo = header.value("solo").toBool();
    track.volume =
        std::clamp(header.value("volume").toDouble(1.0), 0.0, kMaxGain);
  }
  const bool is_too_many = clips.size() > kMaxClipsPerTrack;
  if (is_too_many) {
    issues->Note(Tr("a reel track has too many clips"));
    return track;
  }
  for (const QJsonValue& item : clips) {
    const Clip clip = ClipFromJson(item.toObject(), issues);
    const bool is_kind_right = IsSoundClip(clip) == is_sound &&
                               (IsTextClip(clip) || !track.is_subtitle);
    if (!is_kind_right) {
      issues->Note(Tr("a clip is on the wrong kind of track"));
      continue;
    }
    const bool can_place = clip.length.index() >= 1 && HasRoom(track, clip);
    if (!can_place) {
      issues->Note(Tr("clips overlap on a reel track"));
      continue;
    }
    PlaceClip(&track, clip);
  }
  return track;
}

}  // namespace

QJsonArray ReelToJson(const Reel& reel) {
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxAllTracks));
  assert(kMaxClipsPerTrack > 0);
  QJsonArray tracks;
  for (const ReelTrack& track : reel.tracks) {
    QJsonArray clips;
    for (const Clip& clip : track.clips) {
      clips.append(ClipToJson(clip));
    }
    if (track.is_subtitle) {
      tracks.append(QJsonObject{{"subtitles", true}, {"clips", clips}});
      continue;
    }
    if (!track.is_sound) {
      tracks.append(clips);
      continue;
    }
    tracks.append(QJsonObject{{"sound", true},
                              {"name", track.name},
                              {"muted", track.is_muted},
                              {"solo", track.is_solo},
                              {"volume", track.volume},
                              {"clips", clips}});
  }
  return tracks;
}

Reel ReelFromJson(const QJsonValue& value, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(kMaxReelTracks >= kDefaultReelTracks);
  const bool is_missing = value.isUndefined();
  if (is_missing) {
    return Reel();
  }
  const QJsonArray tracks = value.toArray();
  const bool is_valid = value.isArray() && !tracks.isEmpty() &&
                        tracks.size() <= kMaxAllTracks;
  if (!is_valid) {
    issues->Note(Tr("the reel has no tracks or too many"));
    return Reel();
  }
  Reel reel;
  reel.tracks.clear();
  for (const QJsonValue& track : tracks) {
    reel.tracks.push_back(TrackFromJson(track, issues));
  }
  // Sound tracks sit under the picture ones and the one subtitle track
  // above them all, whatever the file says; a second one is a picture
  // track.
  std::ranges::stable_partition(
      reel.tracks, [](const ReelTrack& track) { return track.is_sound; });
  const int subtitles = SubtitleTrack(reel);
  for (ReelTrack& track : reel.tracks) {
    track.is_subtitle = track.is_subtitle &&
                        &track == &reel.tracks[static_cast<size_t>(
                                      std::max(subtitles, 0))];
  }
  std::ranges::stable_partition(
      reel.tracks, [](const ReelTrack& track) { return !track.is_subtitle; });
  const bool has_picture = PictureTrackCount(reel) > 0;
  if (!has_picture) {
    reel.tracks.insert(reel.tracks.begin() + SoundTrackCount(reel),
                       ReelTrack());
  }
  return reel;
}

QJsonObject ReelTextToJson(const Reel& reel) {
  assert(reel.text_styles.size() <= static_cast<size_t>(kMaxTextStyles));
  assert(reel.subtitle_lines.size() <=
         static_cast<size_t>(kMaxSubtitleLines));
  QJsonArray styles;
  for (const TextStyle& style : reel.text_styles) {
    styles.append(
        QJsonObject{{"name", style.name}, {"look", LookToJson(style.look)}});
  }
  QJsonArray lines;
  for (const QString& line : reel.subtitle_lines) {
    lines.append(line);
  }
  return QJsonObject{{"subtitle_look", LookToJson(reel.subtitle_look)},
                     {"styles", styles},
                     {"lines", lines}};
}

void ReelTextFromJson(const QJsonValue& value, Reel* reel,
                      JsonIssues* issues) {
  assert(reel != nullptr);
  assert(issues != nullptr);
  const QJsonObject object = value.toObject();
  reel->subtitle_look = LookFromJson(object.value("subtitle_look"),
                                     Reel::DefaultSubtitleLook(), issues);
  for (const QJsonValue& item :
       Bounded(object.value("styles"), kMaxTextStyles, "text styles",
               issues)) {
    const QJsonObject style = item.toObject();
    const QString name = style.value("name").toString();
    const bool is_new = !name.isEmpty() && FindTextStyle(*reel, name) == nullptr;
    if (is_new) {
      reel->text_styles.push_back(
          {name, LookFromJson(style.value("look"), ReelTextLook(), issues)});
    }
  }
  for (const QJsonValue& item : Bounded(object.value("lines"),
                                        kMaxSubtitleLines, "subtitle lines",
                                        issues)) {
    reel->subtitle_lines.push_back(item.toString());
  }
}

QJsonArray MarkersToJson(const std::vector<Frame>& markers) {
  assert(markers.size() <= static_cast<size_t>(kMaxCutMarkers));
  assert(std::is_sorted(markers.begin(), markers.end()));
  QJsonArray array;
  for (const Frame marker : markers) {
    array.append(marker.index());
  }
  return array;
}

std::vector<Frame> MarkersFromJson(const QJsonValue& value,
                                   JsonIssues* issues) {
  assert(issues != nullptr);
  assert(kMaxCutMarkers > 0);
  std::vector<Frame> markers;
  const QJsonArray array = value.toArray();
  const bool is_too_many = array.size() > kMaxCutMarkers;
  if (is_too_many) {
    issues->Note(Tr("too many cut markers"));
    return markers;
  }
  for (const QJsonValue& item : array) {
    markers.push_back(Frame(item.toInt()));
  }
  std::sort(markers.begin(), markers.end());
  markers.erase(std::unique(markers.begin(), markers.end()), markers.end());
  return markers;
}

void MoveShotTransitions(const QJsonArray& shots, Reel* reel,
                         JsonIssues* issues) {
  assert(reel != nullptr);
  assert(issues != nullptr);
  const bool is_too_many = shots.size() > kMaxShots;
  if (is_too_many) {
    return;
  }
  for (const QJsonValue& item : shots) {
    const QJsonObject shot = item.toObject();
    const bool is_old = shot.contains("transition");
    if (!is_old) {
      continue;
    }
    const Transition transition =
        TransitionFromJson(shot.value("transition"), issues);
    const ShotSource source{ShotId(shot.value("id").toInt())};
    for (ReelTrack& track : reel->tracks) {
      for (Clip& clip : track.clips) {
        const bool is_its_cut = clip.source == ClipSource(source) &&
                                NextTouching(track, clip) != nullptr &&
                                clip.out.kind == TransitionKind::kCut;
        if (is_its_cut) {
          clip.out = transition;
        }
      }
    }
  }
}

}  // namespace snapper
