#ifndef SNAPPER_EDIT_REEL_MANAGER_H_
#define SNAPPER_EDIT_REEL_MANAGER_H_

#include <QString>

#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "edit/reel_edits.h"
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
  // A video file as a clip source, its length read now.
  Result<VideoSource> ReadVideo(const QString& path) const;
  // Fits source into the slot around at (between two cut markers) on
  // track, showing it from in (kept so the source covers the slot). The
  // clip already filling exactly that slot gets the new source and in;
  // otherwise whatever is on track inside the slot is cut away (clips
  // crossing its ends are split) and a new clip is laid over it.
  Result<ClipId> FillSlot(int track, Frame at, const ClipSource& source,
                          Frame in);
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
  // What a split at at cuts: clips, or with none picked every clip at
  // is inside, on any track.
  std::vector<ClipId> SplitTargets(const std::vector<ClipId>& clips,
                                   Frame at) const;
  // Splits each of SplitTargets that at is inside, as one step; returns
  // the new right halves.
  Result<std::vector<ClipId>> SplitAll(const std::vector<ClipId>& clips,
                                       Frame at);
  Result<void> RemoveAll(const std::vector<ClipId>& clips);
  // Removes clips and closes the gaps they leave: later clips on the
  // same track slide left by what was taken out before them.
  Result<void> RippleDeleteAll(const std::vector<ClipId>& clips);

  // The clipboard. Copies keep their tracks and spacing; a paste lays
  // them down again from at, the first copy starting there.
  Result<void> Copy(const std::vector<ClipId>& clips);
  // Copy, then remove, as one step.
  Result<void> CutAll(const std::vector<ClipId>& clips);
  // Returns the pasted clips.
  Result<std::vector<ClipId>> Paste(Frame at);
  // Copies of clips laid right after the last of them, on the same
  // tracks; returns the copies.
  Result<std::vector<ClipId>> DuplicateAll(const std::vector<ClipId>& clips);
  // A new empty track on top.
  Result<void> AddTrack();
  // Only an empty track goes, and the reel keeps at least one.
  Result<void> RemoveTrack(int track);

  // Drops a cut marker at at, or takes away the one already there.
  Result<void> ToggleMarker(Frame at);

  // Why each can't act now, for greyed-out controls; empty when it can.
  QString WhyNoSplit(const std::vector<ClipId>& clips, Frame at) const;
  // For copy, cut, remove and ripple delete: something is picked.
  QString WhyNoPick(const std::vector<ClipId>& clips) const;
  QString WhyNoPaste(Frame at) const;
  QString WhyNoDuplicate(const std::vector<ClipId>& clips) const;
  QString WhyNoAddTrack() const;
  QString WhyNoRemoveTrack(int track) const;

 private:
  HistoryManager* history_;
  // Copied clips, starts counted from the first one's.
  std::vector<Lifted> clipboard_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_REEL_MANAGER_H_
