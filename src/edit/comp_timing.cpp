// CompManager: when an effect runs, its clip on the Comp timeline.

#include <algorithm>
#include <cassert>

#include "base/text.h"
#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "model/project.h"

namespace snapper {
namespace {

// The longest an effect clip gets, in frames.
constexpr int kMostFrames = 1 << 20;

// Moves every key of effect by delta frames (none goes below frame 0).
void ShiftKeys(CompEffect* effect, int delta) {
  assert(effect != nullptr);
  assert(delta > -kMostFrames && delta < kMostFrames);
  for (Channel<double>& channel : effect->knobs) {
    for (Key<double>& key : channel.keys) {
      key.frame = Frame(key.frame.index() + delta);
    }
  }
}

}  // namespace

QString CompManager::WhyNoTiming(ShotId shot, CompEffectId id) const {
  assert(history_ != nullptr);
  assert(id.value() >= 0);
  const bool is_there = Find(shot, id) != nullptr;
  return is_there ? QString() : Tr("That effect no longer exists.");
}

Result<void> CompManager::SetTiming(ShotId shot, CompEffectId id, int start,
                                    int length, bool carries_keys) {
  assert(history_ != nullptr);
  assert(length == kWholeShot || length >= 0);
  const bool is_sane = start > -kMostFrames && start < kMostFrames &&
                       length < kMostFrames;
  if (!is_sane) {
    return std::unexpected(Error{Tr("That timing is out of range.")});
  }
  const bool is_whole = length == kWholeShot;
  const int frames = is_whole ? 0 : std::max(1, length);
  return Edit(shot, id, is_whole ? Tr("Fit effect to shot")
                                 : Tr("Time effect"),
              [&](CompEffect* effect) -> Result<void> {
                const int delta = (is_whole ? 0 : start) - effect->start;
                const bool is_moved = carries_keys && delta != 0;
                if (is_moved) {
                  ShiftKeys(effect, delta);
                }
                effect->start = is_whole ? 0 : start;
                effect->length = is_whole ? kWholeShot : frames;
                return {};
              });
}

}  // namespace snapper
