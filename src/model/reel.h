#ifndef SNAPPER_MODEL_REEL_H_
#define SNAPPER_MODEL_REEL_H_

#include <QString>

#include <variant>
#include <vector>

#include "base/frame.h"
#include "base/id.h"
#include "model/shot.h"

namespace snapper {

constexpr int kDefaultReelTracks = 3;
constexpr int kMaxReelTracks = 8;
constexpr int kMaxClipsPerTrack = 1024;

struct ClipTag;
using ClipId = Id<ClipTag>;

// A video file played as it is: a speedpaint, a recorded take.
struct VideoSource final {
  QString path;
  // The whole file, read once when it was added.
  Frame length;

  bool operator==(const VideoSource&) const = default;
};

// A shot from the shot strip played as a finished video. It is not a
// copy: fixing the shot fixes every clip of it.
struct ShotSource final {
  ShotId shot;

  bool operator==(const ShotSource&) const = default;
};

// A stretch of a source placed on the reel. It shows the source from
// in for length frames, starting at reel frame start.
struct Clip final {
  ClipId id;
  std::variant<VideoSource, ShotSource> source;
  Frame start;
  Frame in;
  Frame length = Frame(1);

  Frame end() const { return Frame(start.index() + length.index()); }
  bool operator==(const Clip&) const = default;
};

// One row of the reel. Its clips are sorted by start and never overlap.
struct ReelTrack final {
  std::vector<Clip> clips;

  bool operator==(const ReelTrack&) const = default;
};

// The final video: tracks of clips, bottom to top. A higher track's
// clip is drawn over a lower one's, so a shot dropped above a
// speedpaint covers it while it plays.
struct Reel final {
  std::vector<ReelTrack> tracks =
      std::vector<ReelTrack>(static_cast<size_t>(kDefaultReelTracks));

  bool operator==(const Reel&) const = default;
};

// Where a clip sits: its track and its place in that track, or -1s.
struct ClipSpot final {
  int track = -1;
  int index = -1;

  bool IsValid() const { return track >= 0 && index >= 0; }
};

ClipSpot FindClip(const Reel& reel, ClipId id);
const Clip* ClipOf(const Reel& reel, ClipId id);
// True when clip fits on track without overlapping any clip other than
// itself (same id).
bool HasRoom(const ReelTrack& track, const Clip& clip);
// Puts clip into track at its place by start; the caller checked room.
void PlaceClip(ReelTrack* track, Clip clip);
bool IsReelEmpty(const Reel& reel);

}  // namespace snapper

#endif  // SNAPPER_MODEL_REEL_H_
