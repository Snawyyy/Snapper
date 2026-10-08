#include "io/reel_json.h"

#include <QJsonObject>

#include <algorithm>
#include <cassert>
#include <variant>

#include "base/text.h"

namespace snapper {
namespace {

QJsonObject ClipToJson(const Clip& clip) {
  assert(clip.id.IsValid());
  assert(clip.length.index() >= 1);
  QJsonObject object{{"id", clip.id.value()},
                     {"start", clip.start.index()},
                     {"in", clip.in.index()},
                     {"length", clip.length.index()}};
  const auto* video = std::get_if<VideoSource>(&clip.source);
  const bool is_video = video != nullptr;
  if (is_video) {
    object.insert("video", video->path);
    object.insert("video_length", video->length.index());
  } else {
    object.insert("shot", std::get<ShotSource>(clip.source).shot.value());
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
  if (is_video) {
    clip.source = VideoSource{object.value("video").toString(),
                              Frame(object.value("video_length").toInt())};
  } else {
    clip.source = ShotSource{ShotId(object.value("shot").toInt())};
  }
  const bool is_valid = clip.id.IsValid() && clip.length.index() >= 1;
  if (!is_valid) {
    issues->Note(Tr("a clip has no id or no length"));
  }
  return clip;
}

ReelTrack TrackFromJson(const QJsonArray& clips, JsonIssues* issues) {
  assert(issues != nullptr);
  assert(clips.size() >= 0);
  ReelTrack track;
  const bool is_too_many = clips.size() > kMaxClipsPerTrack;
  if (is_too_many) {
    issues->Note(Tr("a reel track has too many clips"));
    return track;
  }
  for (const QJsonValue& item : clips) {
    const Clip clip = ClipFromJson(item.toObject(), issues);
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
  assert(reel.tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(kMaxClipsPerTrack > 0);
  QJsonArray tracks;
  for (const ReelTrack& track : reel.tracks) {
    QJsonArray clips;
    for (const Clip& clip : track.clips) {
      clips.append(ClipToJson(clip));
    }
    tracks.append(clips);
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
                        tracks.size() <= kMaxReelTracks;
  if (!is_valid) {
    issues->Note(Tr("the reel has no tracks or too many"));
    return Reel();
  }
  Reel reel;
  reel.tracks.clear();
  for (const QJsonValue& track : tracks) {
    reel.tracks.push_back(TrackFromJson(track.toArray(), issues));
  }
  return reel;
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

}  // namespace snapper
