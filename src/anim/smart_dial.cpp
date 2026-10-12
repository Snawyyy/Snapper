#include "anim/smart_dial.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/sampler.h"

namespace snapper {
namespace {

// The drawing index of piece called name, or -1.
int DrawingIndex(const Doll& doll, const QString& piece,
                 const QString& name) {
  assert(!piece.isEmpty());
  const ArtPiece* art = FindArt(doll, piece);
  const bool has_art = art != nullptr && !name.isEmpty();
  if (!has_art) {
    return -1;
  }
  const int count = static_cast<int>(art->drawings.size());
  for (int i = 0; i < count; ++i) {
    if (DrawingName(*art, i) == name) {
      return i;
    }
  }
  return -1;
}

// stop's pose for piece, rest when it names none.
PiecePose StopPose(const DialStop& stop, const QString& piece) {
  const auto found = stop.poses.find(piece);
  return found == stop.poses.end() ? PiecePose() : found->second;
}

}  // namespace

PiecePose DialUnder(const PiecePose& change, const PiecePose& hand) {
  PiecePose out = hand;
  out.rotation = change.rotation + hand.rotation;
  out.offset = change.offset + hand.offset;
  out.scale_x = change.scale_x * hand.scale_x;
  out.scale_y = change.scale_y * hand.scale_y;
  out.skew = change.skew + hand.skew;
  out.opacity = change.opacity * hand.opacity;
  out.order = change.order + hand.order;
  out.drawing = hand.drawing >= 0 ? hand.drawing : change.drawing;
  return out;
}

DialSpot SpotOf(const SmartDial& dial, double value) {
  assert(std::isfinite(value));
  assert(dial.stops.size() <= static_cast<size_t>(kMaxDialStops));
  const int count = static_cast<int>(dial.stops.size());
  if (count == 0) {
    return DialSpot{};
  }
  const auto at = [&dial](int i) {
    return dial.stops[static_cast<size_t>(i)].at;
  };
  if (value <= at(0)) {
    return DialSpot{0, 0, 0.0};
  }
  if (value >= at(count - 1)) {
    return DialSpot{count - 1, count - 1, 0.0};
  }
  int to = 1;
  while (to < count - 1 && at(to) < value) {
    ++to;
  }
  const double span = at(to) - at(to - 1);
  const double t = span > 0.0 ? (value - at(to - 1)) / span : 0.0;
  return DialSpot{to - 1, to, std::clamp(t, 0.0, 1.0)};
}

std::map<QString, PiecePose> DialPoses(const Doll& doll,
                                       const SmartDial& dial, double value) {
  assert(std::isfinite(value));
  assert(dial.pieces.size() <= static_cast<size_t>(kMaxDialPieces));
  std::map<QString, PiecePose> poses;
  const DialSpot spot = SpotOf(dial, value);
  if (spot.from < 0) {
    return poses;
  }
  const DialStop& from = dial.stops[static_cast<size_t>(spot.from)];
  const DialStop& to = dial.stops[static_cast<size_t>(spot.to)];
  const bool is_past_swap = dial.swap == DialSwap::kHalfway
                                ? spot.t >= 0.5
                                : spot.t >= 1.0;
  const DialStop& shown = is_past_swap ? to : from;
  for (const QString& piece : dial.pieces) {
    PiecePose pose = Lerp(StopPose(from, piece), StopPose(to, piece), spot.t);
    pose.order = shown.poses.contains(piece) ? StopPose(shown, piece).order
                                             : 0;
    const auto drawing = shown.drawings.find(piece);
    pose.drawing = drawing == shown.drawings.end()
                       ? -1
                       : DrawingIndex(doll, piece, drawing->second);
    pose.warp.clear();
    poses[piece] = pose;
  }
  return poses;
}

double DialValue(const SmartDial& dial, const DollLayer& layer, Frame frame) {
  assert(frame.index() >= 0);
  assert(!dial.name.isEmpty());
  const auto found = layer.dials.find(dial.name);
  const double rest = RestValue(dial);
  const bool has_keys = found != layer.dials.end();
  const double value = has_keys ? Sample(found->second, frame, rest) : rest;
  return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : rest;
}

void ApplyDials(const Doll& doll, const DollLayer& layer, Frame frame,
                std::map<QString, PiecePose>* poses) {
  assert(poses != nullptr);
  assert(frame.index() >= 0);
  const bool has_dials = !doll.rig.dials.empty();
  if (!has_dials) {
    return;
  }
  // Every dial's change, summed per piece; a later dial's drawing wins.
  std::map<QString, PiecePose> changes;
  for (const SmartDial& dial : doll.rig.dials) {
    const double value = DialValue(dial, layer, frame);
    for (const auto& [piece, pose] : DialPoses(doll, dial, value)) {
      const auto had = changes.find(piece);
      const bool is_first = had == changes.end();
      if (is_first) {
        changes[piece] = pose;
        continue;
      }
      had->second = DialUnder(had->second, pose);
    }
  }
  for (const auto& [piece, change] : changes) {
    const bool is_rigged = FindRig(doll.rig, piece) != nullptr;
    if (is_rigged) {
      PiecePose& hand = (*poses)[piece];
      hand = DialUnder(change, hand);
    }
  }
}

double SnappedDialValue(const SmartDial& dial, double value) {
  assert(std::isfinite(value));
  assert(kDialSnap > 0.0);
  double best = value;
  double nearest = kDialSnap;
  for (const DialStop& stop : dial.stops) {
    const double apart = std::abs(stop.at - value);
    if (apart <= nearest) {
      nearest = apart;
      best = stop.at;
    }
  }
  return std::clamp(best, 0.0, 1.0);
}

QString DialText(const SmartDial& dial, double value) {
  assert(std::isfinite(value));
  const DialSpot spot = SpotOf(dial, value);
  if (spot.from < 0) {
    return QStringLiteral("%1%").arg(std::lround(value * 100.0));
  }
  const DialStop& from = dial.stops[static_cast<size_t>(spot.from)];
  const DialStop& to = dial.stops[static_cast<size_t>(spot.to)];
  const long percent = std::lround(spot.t * 100.0);
  const bool is_on_stop = spot.from == spot.to || percent == 0;
  if (is_on_stop) {
    return from.name;
  }
  if (percent == 100) {
    return to.name;
  }
  return QStringLiteral("%1 %2% %3").arg(from.name).arg(percent).arg(to.name);
}

}  // namespace snapper
