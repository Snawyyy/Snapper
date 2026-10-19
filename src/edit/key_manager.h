#ifndef SNAPPER_EDIT_KEY_MANAGER_H_
#define SNAPPER_EDIT_KEY_MANAGER_H_

#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>

#include "anim/timing_chart.h"
#include "base/error.h"
#include "base/frame.h"
#include "edit/track_ref.h"
#include "model/key.h"
#include "model/pose.h"

namespace snapper {

class HistoryManager;

// Copied keys of one channel, with frames counted from the first copied
// key.
using CopiedChannel = std::variant<Channel<PiecePose>, Channel<CameraPose>,
                                   Channel<double>, Channel<SkinSet>,
                                   Channel<CharacterPose>>;

// Timing: keys on the timeline, whatever channel they are in.
class KeyManager final {
 public:
  explicit KeyManager(HistoryManager* history);

  // Slides keys by delta frames; a key landing on an unpicked one
  // replaces it. Returns where the keys ended up, to keep them picked.
  Result<std::set<KeyRef>> Shift(const std::set<KeyRef>& keys, int delta);
  // Spaces the picked keys' frames step frames apart (on 2s, on 3s...)
  // from the first one, keeping their order. Unpicked keys after the
  // last picked one on the same channels slide with it, so the timing
  // after stays the same. Returns where the keys ended up.
  Result<std::set<KeyRef>> Space(const std::set<KeyRef>& keys, int step);
  // Why Space can't act on keys; empty when it can.
  QString WhyNoSpace(const std::set<KeyRef>& keys) const;
  Result<void> Remove(const std::set<KeyRef>& keys);
  Result<void> SetEase(const std::set<KeyRef>& keys, Ease ease);
  // The ease every picked key has, or nothing when they differ or none
  // is picked.
  std::optional<Ease> EaseOf(const std::set<KeyRef>& keys) const;
  // Holds the picked keys' last pose delta frames longer (above 0) or
  // shorter (below 0): frames go in or out right after the last picked
  // key on the picked keys' channels, and later keys slide.
  Result<void> Hold(const std::set<KeyRef>& keys, int delta);
  // Why Hold can't act; empty when it can.
  QString WhyNoHold(const std::set<KeyRef>& keys, int delta) const;
  // Adds (delta > 0) or takes out (delta < 0) frames at frame on every
  // channel in tracks: later keys slide, keys inside a taken-out span
  // go. This is how a hold is made longer or shorter.
  Result<void> Retime(const std::vector<TrackRef>& tracks, Frame frame,
                      int delta);
  // Plays the keys in [start, end) times more right after end, sliding
  // later keys out of the way.
  Result<void> Repeat(const std::vector<TrackRef>& tracks, Frame start,
                      Frame end, int times);

  // Bakes a timing chart: on every track, a key on each of frames
  // (strictly between a and b, as TickFrames gives them) at fractions
  // of the way from the pose at a to the pose at b; mode says whether
  // the in-between keys are replaced or only re-posed. No existing key
  // moves in time. Builds on the project a running drag started from,
  // so it can be called again and again for a live preview; lands as
  // one step named label.
  Result<void> BakeTiming(const std::vector<TrackRef>& tracks, Frame a,
                          Frame b, KeyMode mode,
                          const std::vector<int>& frames,
                          const std::vector<double>& fractions,
                          const QString& label);

  Result<void> Copy(const std::set<KeyRef>& keys);
  // Puts the copied keys back with the first at frame, on the same
  // channels in shot. When onto is a layer, layer and piece keys go
  // onto that layer instead.
  Result<void> Paste(ShotId shot, Frame frame, LayerId onto);
  QString WhyNoPaste() const;

 private:
  HistoryManager* history_;
  std::map<TrackRef, CopiedChannel> clipboard_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_KEY_MANAGER_H_
