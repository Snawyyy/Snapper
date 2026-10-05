#include "anim/presets.h"

#include <cassert>
#include <cmath>
#include <cstdint>

#include "base/text.h"

namespace snapper {
namespace {

// Same frame, same jitter: a shake renders identically every time.
double Jitter(int step, int axis) {
  assert(step >= 0);
  assert(axis == 0 || axis == 1);
  std::uint32_t x = static_cast<std::uint32_t>(step * 2 + axis + 1);
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  x *= 2654435761u;
  return (x % 2001u) / 1000.0 - 1.0;
}

// The pose change on the step-th key of a loop.
PiecePose StepPose(const PresetSettings& settings, int step) {
  assert(step >= 0);
  assert(kMotionPresetCount == 5);
  const double amount = settings.amount;
  const bool is_odd = step % 2 == 1;
  PiecePose pose;
  switch (settings.preset) {
    case MotionPreset::kBob:
      pose.offset.setY(is_odd ? amount : 0.0);
      break;
    case MotionPreset::kBounce: {
      // Up, land squashed, settle.
      const int phase = step % 3;
      pose.offset.setY(phase == 0 ? -amount : 0.0);
      pose.scale_y = phase == 1 ? 0.9 : 1.0;
      pose.scale_x = phase == 1 ? 1.1 : 1.0;
      break;
    }
    case MotionPreset::kShake:
      pose.offset = QPointF(Jitter(step, 0), Jitter(step, 1)) * amount;
      break;
    case MotionPreset::kNod:
      pose.rotation = is_odd ? amount : 0.0;
      break;
    case MotionPreset::kSway:
      pose.rotation = is_odd ? amount : -amount;
      break;
  }
  return pose;
}

}  // namespace

QString PresetName(MotionPreset preset) {
  assert(kMotionPresetCount == 5);
  assert(static_cast<int>(preset) < kMotionPresetCount);
  switch (preset) {
    case MotionPreset::kBob:
      return Tr("Bob");
    case MotionPreset::kBounce:
      return Tr("Bounce");
    case MotionPreset::kShake:
      return Tr("Shake");
    case MotionPreset::kNod:
      return Tr("Nod");
    case MotionPreset::kSway:
      return Tr("Sway");
  }
  return QString();
}

std::vector<Key<PiecePose>> PresetKeys(const PresetSettings& settings) {
  assert(settings.hold >= 1);
  assert(settings.length >= 0 && settings.length <= kMaxFrame);
  std::vector<Key<PiecePose>> keys;
  const bool is_valid = settings.hold >= 1 && settings.length > 0;
  if (!is_valid) {
    return keys;
  }
  // Sway glides; the rest snap.
  const Ease ease = settings.preset == MotionPreset::kSway ? Ease::kEaseInOut
                                                           : Ease::kStep;
  const int steps = (settings.length + settings.hold - 1) / settings.hold;
  keys.reserve(static_cast<size_t>(steps));
  for (int step = 0; step < steps; ++step) {
    keys.push_back(
        {Frame(step * settings.hold), StepPose(settings, step), ease});
  }
  return keys;
}

PiecePose AddPose(const PiecePose& base, const PiecePose& change) {
  assert(std::isfinite(change.rotation));
  assert(std::isfinite(change.scale_x) && std::isfinite(change.scale_y));
  PiecePose out = base;
  out.rotation += change.rotation;
  out.offset += change.offset;
  out.skew += change.skew;
  out.scale_x *= change.scale_x;
  out.scale_y *= change.scale_y;
  return out;
}

}  // namespace snapper
