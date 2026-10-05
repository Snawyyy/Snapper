#ifndef SNAPPER_ANIM_SAMPLER_H_
#define SNAPPER_ANIM_SAMPLER_H_

#include <QPointF>

#include <algorithm>
#include <cassert>

#include "base/frame.h"
#include "model/key.h"
#include "model/pose.h"

namespace snapper {

// How far along (0 to 1) a key's ease is at t (0 to 1).
double EaseAmount(Ease ease, double t);

// The in-betweens. Values that can't blend (a drawing choice) hold the
// first value until the next key.
double Lerp(double a, double b, double t);
QPointF Lerp(QPointF a, QPointF b, double t);
PiecePose Lerp(const PiecePose& a, const PiecePose& b, double t);
CameraPose Lerp(const CameraPose& a, const CameraPose& b, double t);

// The one sampler: the value of channel at frame. Before the first key
// it holds the first key, after the last it holds the last, and with no
// keys it is fallback.
template <typename T>
T Sample(const Channel<T>& channel, Frame frame, const T& fallback) {
  assert(IsSorted(channel));
  assert(frame.index() >= 0);
  const auto& keys = channel.keys;
  const bool has_no_keys = keys.empty();
  if (has_no_keys) {
    return fallback;
  }
  const auto next = std::upper_bound(
      keys.begin(), keys.end(), frame,
      [](Frame at, const Key<T>& key) { return at < key.frame; });
  const bool is_before_first = next == keys.begin();
  if (is_before_first) {
    return keys.front().value;
  }
  const auto& from = *(next - 1);
  const bool is_holding = next == keys.end() || from.ease == Ease::kStep;
  if (is_holding) {
    return from.value;
  }
  const double span = next->frame.index() - from.frame.index();
  const double t = (frame.index() - from.frame.index()) / span;
  return Lerp(from.value, next->value, EaseAmount(from.ease, t));
}

}  // namespace snapper

#endif  // SNAPPER_ANIM_SAMPLER_H_
