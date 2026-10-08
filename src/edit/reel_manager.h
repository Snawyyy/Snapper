#ifndef SNAPPER_EDIT_REEL_MANAGER_H_
#define SNAPPER_EDIT_REEL_MANAGER_H_

#include <QString>

#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "model/reel.h"
#include "model/shot.h"

namespace snapper {

class HistoryManager;

// The reel, the final video: which clips are on which track, where and
// how much of their source each shows. A shot clip is a finished video
// here; what is inside it is edited only on the shot itself.
class ReelManager final {
 public:
  explicit ReelManager(HistoryManager* history);

  // A whole video file onto track at start. Its length is read now.
  Result<ClipId> AddVideo(const QString& path, int track, Frame start);
  // A whole shot onto track at start.
  Result<ClipId> AddShot(ShotId shot, int track, Frame start);
  // Moves every clip in clips by tracks rows and frames frames from
  // where the open drag started (HistoryManager::before), so a drag can
  // call it with its running total. Refused when any would leave the
  // reel or land on another clip.
  Result<void> MoveAll(const std::vector<ClipId>& clips, int tracks,
                       int frames);
  // Drags the clip's left edge to start, keeping its end: more or less of
  // its source shows at the front.
  Result<void> TrimStart(ClipId clip, Frame start);
  // Drags the clip's right edge to end, keeping its start.
  Result<void> TrimEnd(ClipId clip, Frame end);
  // Cuts the clip in two at reel frame at, strictly inside it. Returns
  // the new right half.
  Result<ClipId> Split(ClipId clip, Frame at);
  // Splits each of clips that at is inside, as one step; returns the
  // new right halves.
  Result<std::vector<ClipId>> SplitAll(const std::vector<ClipId>& clips,
                                       Frame at);
  Result<void> RemoveAll(const std::vector<ClipId>& clips);
  // A new empty track on top.
  Result<void> AddTrack();
  // Only an empty track goes, and the reel keeps at least one.
  Result<void> RemoveTrack(int track);

  // Drops a cut marker at at, or takes away the one already there.
  Result<void> ToggleMarker(Frame at);

  // Why each can't act now, for greyed-out controls; empty when it can.
  QString WhyNoSplit(const std::vector<ClipId>& clips, Frame at) const;
  QString WhyNoAddTrack() const;
  QString WhyNoRemoveTrack(int track) const;

 private:
  HistoryManager* history_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_REEL_MANAGER_H_
