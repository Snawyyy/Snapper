#ifndef SNAPPER_ANIM_PRESETS_H_
#define SNAPPER_ANIM_PRESETS_H_

#include <QString>

#include <vector>

#include "model/key.h"
#include "model/pose.h"

namespace snapper {

enum class MotionPreset { kBob, kBounce, kShake, kNod, kSway };
constexpr int kMotionPresetCount = 5;

// A loop to lay onto a piece or layer.
struct PresetSettings final {
  MotionPreset preset = MotionPreset::kBob;
  // Pixels for moves, degrees for turns.
  double amount = 10.0;
  // Frames each pose holds: 2 for on-2s, 3 for on-3s.
  int hold = 2;
  // Frames the loop covers.
  int length = 24;
};

QString PresetName(MotionPreset preset);

// The loop as keys of pose changes, starting at frame 0. Each key's
// value is added onto whatever pose is already there (AddPose).
std::vector<Key<PiecePose>> PresetKeys(const PresetSettings& settings);

// base moved by change: turns, moves and leans add; scales multiply.
PiecePose AddPose(const PiecePose& base, const PiecePose& change);

}  // namespace snapper

#endif  // SNAPPER_ANIM_PRESETS_H_
