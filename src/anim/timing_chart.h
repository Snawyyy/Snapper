#ifndef SNAPPER_ANIM_TIMING_CHART_H_
#define SNAPPER_ANIM_TIMING_CHART_H_

#include <algorithm>
#include <cassert>
#include <vector>

#include "anim/sampler.h"
#include "base/frame.h"
#include "model/key.h"

namespace snapper {

// A timing chart is the classic spacing diagram: pose A and pose B at
// the ends, one tick for every frame between. A tick's fraction (0 to 1)
// says how far from A to B the thing is on that frame. Frames never
// move; only the pose on each in-between frame is set.

// Ticks keep at least this much of the line between neighbours, so they
// never cross or touch A and B.
constexpr double kMinTickGap = 0.01;
// The longest stretch a chart covers, A to B, in frames.
constexpr int kMaxChartFrames = 96;

// The ready-made spacings. Slow in starts gently (ticks bunch at A),
// slow out lands gently (ticks bunch at B); halves keep halving the
// distance to the favoured end.
enum class Spacing {
  kEven,
  kSlowIn,
  kSlowOut,
  kSlowInOut,
  kHalvesA,
  kHalvesB
};
constexpr int kSpacingCount = 6;

// The fractions of count in-between ticks for a spacing, rising and
// inside (0, 1).
std::vector<double> SpacingFractions(Spacing spacing, int count);

// fractions with tick index dragged to wanted, held between its
// neighbours (or A and B) by kMinTickGap.
std::vector<double> MoveTick(std::vector<double> fractions, int index,
                             double wanted);

// The tick nearest to fraction within reach (a share of the line), or
// -1 when none is that close; the first of two equally near wins.
int NearestTick(const std::vector<double>& fractions, double fraction,
                double reach);

// True when fractions is rising and inside (0, 1) with the minimum gap.
bool IsValidSpacing(const std::vector<double>& fractions);

// Bakes the chart into channel: a key on every frame from a to b. A
// and B keep the pose the channel shows there now, each in-between
// frame gets the blend at its fraction, and the keys that were between
// are replaced. B keeps its own ease; the rest step, since each key
// holds exactly one frame. Keys outside a..b are untouched. A channel
// with no keys has no pose to follow and is left alone. False when the
// channel has no room.
template <typename T>
bool BakeChannel(Channel<T>* channel, Frame a, Frame b,
                 const std::vector<double>& fractions) {
  assert(channel != nullptr);
  assert(static_cast<int>(fractions.size()) == b.index() - a.index() - 1);
  const bool has_keys = !channel->keys.empty();
  if (!has_keys) {
    return true;
  }
  const T from = Sample(*channel, a, T());
  const T to = Sample(*channel, b, T());
  const int b_at = KeyIndexAt(*channel, b);
  const Ease b_ease =
      b_at >= 0 ? channel->keys[static_cast<size_t>(b_at)].ease : Ease::kStep;
  std::erase_if(channel->keys, [&](const Key<T>& key) {
    return key.frame.index() >= a.index() && key.frame.index() <= b.index();
  });
  bool is_set = SetKey(channel, Key<T>{a, from, Ease::kStep});
  for (size_t i = 0; i < fractions.size(); ++i) {
    const Frame frame(a.index() + 1 + static_cast<int>(i));
    is_set = SetKey(channel, Key<T>{frame, Lerp(from, to, fractions[i]),
                                    Ease::kStep}) &&
             is_set;
  }
  return SetKey(channel, Key<T>{b, to, b_ease}) && is_set;
}

}  // namespace snapper

#endif  // SNAPPER_ANIM_TIMING_CHART_H_
