#ifndef SNAPPER_MODEL_TRANSITION_H_
#define SNAPPER_MODEL_TRANSITION_H_

#include "base/frame.h"

namespace snapper {

enum class TransitionKind {
  kCut,
  kSwipeLeft,
  kSwipeRight,
  kSwipeUp,
  kSwipeDown,
  kFlash,
  kCrossfade,
};
constexpr int kTransitionKindCount = 7;

// How one clip hands over to the next one touching it on the reel: the
// two are mixed over the last length frames before the cut.
struct Transition final {
  TransitionKind kind = TransitionKind::kCut;
  Frame length;

  bool operator==(const Transition&) const = default;
};

// The length actually used between clips before and after frames long:
// 0 for a cut, and never so long that either clip is all transition.
Frame UsableLength(const Transition& transition, Frame before, Frame after);

}  // namespace snapper

#endif  // SNAPPER_MODEL_TRANSITION_H_
