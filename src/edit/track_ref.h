#ifndef SNAPPER_EDIT_TRACK_REF_H_
#define SNAPPER_EDIT_TRACK_REF_H_

#include <QString>

#include <compare>
#include <variant>

#include "base/error.h"
#include "base/frame.h"
#include "base/text.h"
#include "model/shot.h"

namespace snapper {

// Which channel of keys: a layer's own move, one doll piece, the
// shot's camera, or an effect's strength.
enum class TrackKind { kLayer, kPiece, kCamera, kEffectAmount };

// The address of one channel, so pose, key and preset tools and the
// selection all point at keys the same way.
struct TrackRef final {
  ShotId shot;
  TrackKind kind = TrackKind::kLayer;
  // Unused for the camera.
  LayerId layer;
  // Only for kPiece.
  QString piece;

  auto operator<=>(const TrackRef&) const = default;
  bool operator==(const TrackRef&) const = default;
};

// One key: its channel and its frame.
struct KeyRef final {
  TrackRef track;
  Frame frame;

  auto operator<=>(const KeyRef&) const = default;
  bool operator==(const KeyRef&) const = default;
};

// Calls visit(Channel<T>*) with the channel track names in shot, which
// is a channel of PiecePose, CameraPose or double. A piece without keys
// yet gets an empty channel to write into. Fails when the layer is gone
// or is the wrong kind for the track.
template <typename Visit>
Result<void> VisitTrack(Shot* shot, const TrackRef& track, Visit visit) {
  assert(shot != nullptr);
  assert(track.shot == shot->id);
  const auto gone = [] {
    return Result<void>(std::unexpected(
        Error{Tr("Those keys belong to something that no longer exists.")}));
  };
  const bool is_camera = track.kind == TrackKind::kCamera;
  if (is_camera) {
    return visit(&shot->camera);
  }
  Layer* layer = FindLayer(shot, track.layer);
  const bool has_layer = layer != nullptr;
  if (!has_layer) {
    return gone();
  }
  const bool is_layer = track.kind == TrackKind::kLayer;
  if (is_layer) {
    return visit(&layer->transform);
  }
  auto* doll = std::get_if<DollLayer>(&layer->content);
  const bool is_piece = track.kind == TrackKind::kPiece && doll != nullptr &&
                        !track.piece.isEmpty();
  if (is_piece) {
    return visit(&doll->pieces[track.piece]);
  }
  auto* effect = std::get_if<EffectLayer>(&layer->content);
  const bool is_effect =
      track.kind == TrackKind::kEffectAmount && effect != nullptr;
  if (is_effect) {
    return visit(&effect->amount);
  }
  return gone();
}

// Read-only twin of VisitTrack: calls read(const Channel<T>&). A piece
// without keys reads as an empty channel. False when the track's layer
// is gone or the wrong kind.
template <typename Read>
bool ReadTrack(const Shot& shot, const TrackRef& track, Read read) {
  assert(track.shot == shot.id);
  assert(track.kind != TrackKind::kPiece || !track.piece.isEmpty());
  const bool is_camera = track.kind == TrackKind::kCamera;
  if (is_camera) {
    read(shot.camera);
    return true;
  }
  const Layer* layer = FindLayer(shot, track.layer);
  const bool has_layer = layer != nullptr;
  if (!has_layer) {
    return false;
  }
  const bool is_layer = track.kind == TrackKind::kLayer;
  if (is_layer) {
    read(layer->transform);
    return true;
  }
  const auto* doll = std::get_if<DollLayer>(&layer->content);
  const bool is_piece = track.kind == TrackKind::kPiece && doll != nullptr;
  if (is_piece) {
    const auto found = doll->pieces.find(track.piece);
    const bool has_keys = found != doll->pieces.end();
    if (has_keys) {
      read(found->second);
    } else {
      read(Channel<PiecePose>());
    }
    return true;
  }
  const auto* effect = std::get_if<EffectLayer>(&layer->content);
  const bool is_effect =
      track.kind == TrackKind::kEffectAmount && effect != nullptr;
  if (is_effect) {
    read(effect->amount);
  }
  return is_effect;
}

}  // namespace snapper

#endif  // SNAPPER_EDIT_TRACK_REF_H_
