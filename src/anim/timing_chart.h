#ifndef SNAPPER_ANIM_TIMING_CHART_H_
#define SNAPPER_ANIM_TIMING_CHART_H_

#include <algorithm>
#include <cassert>
#include <string_view>
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

// Which frames a chart keys. Existing keys only never adds a key: its
// ticks are the keys already between A and B. The others key every 1st,
// 2nd or 3rd frame from A (B is always keyed), and drop the in-between
// keys that miss that grid.
enum class KeyMode { kExisting, kOnes, kTwos, kThrees };
constexpr int kKeyModeCount = 4;

// The frames of the ticks, rising, strictly between a and b. existing
// are the frames the thing already has keys on (any order, any range);
// only Existing keys only reads them.
std::vector<int> TickFrames(KeyMode mode, int a, int b,
                            const std::vector<int>& existing);

// The word a mode is saved as, and the mode a saved word names (the
// fallback for anything else).
const char* KeyModeKey(KeyMode mode);
KeyMode KeyModeFromKey(std::string_view key, KeyMode fallback);

// Frames from first up to (not including) last on which channel no
// longer shows what original showed are keyed with original's pose, so
// a chart leaves the motion outside A..B exactly as it was. Does
// nothing when every frame there still matches.
template <typename T>
bool KeepFramesAs(Channel<T>* channel, const Channel<T>& original,
                  int first, int last) {
  assert(channel != nullptr);
  assert(first <= last);
  bool has_drifted = false;
  for (int f = first; f < last; ++f) {
    has_drifted = has_drifted || !(Sample(*channel, Frame(f), T()) ==
                                   Sample(original, Frame(f), T()));
  }
  bool is_set = true;
  for (int f = first; f < last && has_drifted; ++f) {
    is_set = SetKey(channel, Key<T>{Frame(f), Sample(original, Frame(f), T()),
                                    Ease::kStep}) &&
             is_set;
  }
  return is_set;
}

// Bakes the chart into channel: a key on each of frames (the ticks,
// from TickFrames), fraction i of the way from the pose A has to the
// pose B has. A and B keep the pose the channel shows there now, and
// the keys that were between are replaced. Existing keys only leaves
// every key where it is and re-poses just the keys between (keeping
// their ease); it adds a key only on A or B when there is none, holding
// the pose there; the other
// modes step, since each key holds its frames, and B keeps its own
// ease. Keys outside a..b are untouched, and frames outside whose pose
// would change (an eased key next to A or B that had no key there) are
// keyed with the pose they had. A channel with no keys has no pose to
// follow and is left alone. False when the channel has no room.
template <typename T>
bool BakeChannel(Channel<T>* channel, Frame a, Frame b,
                 const std::vector<int>& frames,
                 const std::vector<double>& fractions, KeyMode mode) {
  assert(channel != nullptr);
  assert(frames.size() == fractions.size());
  const bool has_keys = !channel->keys.empty();
  if (!has_keys) {
    return true;
  }
  const Channel<T> original = *channel;
  const T from = Sample(*channel, a, T());
  const T to = Sample(*channel, b, T());
  const int b_at = KeyIndexAt(*channel, b);
  const Ease b_ease =
      b_at >= 0 ? channel->keys[static_cast<size_t>(b_at)].ease : Ease::kStep;
  const bool is_existing = mode == KeyMode::kExisting;
  std::erase_if(channel->keys, [&](const Key<T>& key) {
    const bool is_in_range =
        key.frame.index() >= a.index() && key.frame.index() <= b.index();
    return !is_existing && is_in_range;
  });
  bool is_set = true;
  const bool pins_a = !is_existing || KeyIndexAt(original, a) < 0;
  const bool pins_b = !is_existing || b_at < 0;
  if (pins_a) {
    is_set = SetKey(channel, Key<T>{a, from, Ease::kStep});
  }
  for (size_t i = 0; i < frames.size(); ++i) {
    const Frame frame(frames[i]);
    const int was_at = KeyIndexAt(original, frame);
    const bool is_skipped = is_existing && was_at < 0;
    if (is_skipped) {
      continue;
    }
    const Ease ease =
        is_existing ? original.keys[static_cast<size_t>(was_at)].ease
                    : Ease::kStep;
    is_set = SetKey(channel, Key<T>{frame, Lerp(from, to, fractions[i]),
                                    ease}) &&
             is_set;
  }
  if (pins_b) {
    is_set = SetKey(channel, Key<T>{b, to, b_ease}) && is_set;
  }
  // The keys just before A and after B must not bend differently now.
  const auto before = std::lower_bound(
      original.keys.begin(), original.keys.end(), a,
      [](const Key<T>& key, Frame at) { return key.frame < at; });
  const auto after = std::upper_bound(
      original.keys.begin(), original.keys.end(), b,
      [](Frame at, const Key<T>& key) { return at < key.frame; });
  const int low = before == original.keys.begin()
                      ? a.index()
                      : (before - 1)->frame.index() + 1;
  const int high = after == original.keys.end() ? b.index() + 1
                                                : after->frame.index();
  return KeepFramesAs(channel, original, low, a.index()) &&
         KeepFramesAs(channel, original, b.index() + 1, high) && is_set;
}

}  // namespace snapper

#endif  // SNAPPER_ANIM_TIMING_CHART_H_
