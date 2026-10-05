#include "edit/timeline_rows.h"

#include <cassert>
#include <map>
#include <variant>

#include "base/text.h"

namespace snapper {
namespace {

// A layer's channels: its own move, plus each piece or its strength.
std::vector<TrackRef> LayerTracks(const Shot& shot, const Layer& layer) {
  assert(layer.id.IsValid());
  assert(shot.id.IsValid());
  std::vector<TrackRef> tracks = {
      {shot.id, TrackKind::kLayer, layer.id, {}}};
  const auto* doll = std::get_if<DollLayer>(&layer.content);
  const bool is_doll = doll != nullptr;
  if (is_doll) {
    for (const auto& [piece, channel] : doll->pieces) {
      tracks.push_back({shot.id, TrackKind::kPiece, layer.id, piece});
    }
  }
  const bool is_effect = std::holds_alternative<EffectLayer>(layer.content);
  if (is_effect) {
    tracks.push_back({shot.id, TrackKind::kEffectAmount, layer.id, {}});
  }
  return tracks;
}

// Fills the row's key frames from its channels. A frame eases when any
// key on it does.
void CollectKeys(const Shot& shot, TimelineRow* row) {
  assert(row != nullptr);
  assert(row->keys.empty());
  std::map<Frame, bool> frames;
  for (const TrackRef& track : row->tracks) {
    ReadTrack(shot, track, [&frames](const auto& channel) {
      for (const auto& key : channel.keys) {
        frames[key.frame] = frames[key.frame] || key.ease != Ease::kStep;
      }
    });
  }
  for (const auto& [frame, is_eased] : frames) {
    row->keys.push_back(frame);
    row->is_eased.push_back(is_eased);
  }
}

}  // namespace

std::vector<TimelineRow> TimelineRows(const Project& project, ShotId shot) {
  assert(shot.value() >= 0);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  std::vector<TimelineRow> rows;
  const Shot* found = FindShot(project, shot);
  const bool has_shot = found != nullptr;
  if (!has_shot) {
    return rows;
  }
  for (auto it = found->layers.rbegin(); it != found->layers.rend(); ++it) {
    TimelineRow row{it->name, it->id, LayerTracks(*found, *it), {}, {}};
    CollectKeys(*found, &row);
    rows.push_back(std::move(row));
  }
  TimelineRow camera{Tr("Camera"), LayerId(),
                     {{shot, TrackKind::kCamera, {}, {}}}, {}, {}};
  CollectKeys(*found, &camera);
  rows.push_back(std::move(camera));
  return rows;
}

std::set<KeyRef> RowKeysAt(const Project& project, const TimelineRow& row,
                           Frame frame) {
  assert(frame.index() >= 0);
  assert(!row.tracks.empty());
  std::set<KeyRef> keys;
  const Shot* shot = FindShot(project, row.tracks.front().shot);
  const bool has_shot = shot != nullptr;
  if (!has_shot) {
    return keys;
  }
  for (const TrackRef& track : row.tracks) {
    ReadTrack(*shot, track, [&](const auto& channel) {
      const bool is_keyed = KeyIndexAt(channel, frame) >= 0;
      if (is_keyed) {
        keys.insert({track, frame});
      }
    });
  }
  return keys;
}

}  // namespace snapper
