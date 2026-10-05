#ifndef SNAPPER_EDIT_KEY_MANAGER_H_
#define SNAPPER_EDIT_KEY_MANAGER_H_

#include <map>
#include <set>
#include <variant>
#include <vector>

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
                                   Channel<double>>;

// Timing: keys on the timeline, whatever channel they are in.
class KeyManager final {
 public:
  explicit KeyManager(HistoryManager* history);

  // Slides keys by delta frames; a key landing on an unpicked one
  // replaces it. Returns where the keys ended up, to keep them picked.
  Result<std::set<KeyRef>> Shift(const std::set<KeyRef>& keys, int delta);
  Result<void> Remove(const std::set<KeyRef>& keys);
  Result<void> SetEase(const std::set<KeyRef>& keys, Ease ease);
  // Adds (delta > 0) or takes out (delta < 0) frames at frame on every
  // channel in tracks: later keys slide, keys inside a taken-out span
  // go. This is how a hold is made longer or shorter.
  Result<void> Retime(const std::vector<TrackRef>& tracks, Frame frame,
                      int delta);
  // Plays the keys in [start, end) times more right after end, sliding
  // later keys out of the way.
  Result<void> Repeat(const std::vector<TrackRef>& tracks, Frame start,
                      Frame end, int times);

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
