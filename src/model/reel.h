#ifndef SNAPPER_MODEL_REEL_H_
#define SNAPPER_MODEL_REEL_H_

#include <QColor>
#include <QPointF>
#include <QString>

#include <variant>
#include <vector>

#include "base/frame.h"
#include "base/id.h"
#include "model/layer.h"
#include "model/shot.h"
#include "model/transition.h"

namespace snapper {

constexpr int kDefaultReelTracks = 3;
// Picture tracks; sound tracks count apart (kMaxSoundTracks), and so
// does the one subtitle track.
constexpr int kMaxReelTracks = 8;
constexpr int kDefaultSoundTracks = 2;
constexpr int kMaxSoundTracks = 8;
// Every track of any kind: the most a reel's track list holds.
constexpr int kMaxAllTracks = kMaxReelTracks + kMaxSoundTracks + 1;
constexpr int kMaxTextStyles = 64;
constexpr int kMaxSubtitleLines = 4096;
// A text clip's text, in characters.
constexpr int kMaxReelTextLength = 4096;

// How a text clip appears at its start and leaves at its end.
enum class TextMotion {
  kCut,
  kFade,
  kPop,      // Grows in from small, shrinks away.
  kSlideUp,  // Rises into place, rises away.
  kTypeOn,   // Letters show one by one; leaving, they go the same way.
};
constexpr int kTextMotionCount = 5;

// How text on the reel looks: a shot text layer's lettering (its text
// field unused here, so a look learned in the Pose tab works the same),
// a box behind the text (clear for none), and where its middle sits as
// a share of the frame (0.5, 0.5 is the middle), so a new canvas size
// keeps it in place.
struct ReelTextLook final {
  TextLayer letters;
  QColor box = Qt::transparent;
  QPointF place{0.5, 0.5};

  bool operator==(const ReelTextLook&) const = default;
};

// A look saved under a name ("Title", "Caption"), kept in the project.
struct TextStyle final {
  QString name;
  ReelTextLook look;

  bool operator==(const TextStyle&) const = default;
};

// Words on the reel: a title, a caption, or a subtitle cue on the
// subtitle track. It has no length of its own; the clip sets it.
struct TextSource final {
  QString text;
  ReelTextLook look;
  TextMotion appear = TextMotion::kCut;
  Frame appear_length = Frame(6);
  TextMotion leave = TextMotion::kCut;
  Frame leave_length = Frame(6);
  // The named style it follows; changing that style changes it. Empty
  // when it follows none.
  QString style;
  // Cues only: true when it keeps its own look instead of the subtitle
  // track's.
  bool is_own_look = false;

  bool operator==(const TextSource&) const = default;
};
constexpr int kMaxClipsPerTrack = 1024;
constexpr int kMaxCutMarkers = 2048;
// The loudest a clip or track gets: +6 dB, as a gain.
constexpr double kMaxGain = 1.9952623149688795;

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

// A sound file (WAV, MP3, OGG, FLAC) on a sound track: a song, an
// effect, a voice take. Kept by path, like a video file.
struct SoundSource final {
  QString path;
  // The whole file, read once when it was added.
  Frame length;

  bool operator==(const SoundSource&) const = default;
};

using ClipSource =
    std::variant<VideoSource, ShotSource, SoundSource, TextSource>;

// A stretch of a source placed on the reel. It shows the source from
// in for length frames, starting at reel frame start.
struct Clip final {
  ClipId id;
  ClipSource source;
  Frame start;
  Frame in;
  Frame length = Frame(1);
  // Into the next clip on the track. It plays only while that clip
  // starts where this one ends; pulled apart, the two just cut.
  Transition out;
  // Sound clips only: the clip's gain (1 is as recorded, 0 silent, at
  // most kMaxGain) and how many frames it fades in and out over.
  double volume = 1.0;
  Frame fade_in;
  Frame fade_out;

  Frame end() const { return Frame(start.index() + length.index()); }
  bool operator==(const Clip&) const = default;
};

// One row of the reel. Its clips are sorted by start and never overlap.
// A sound track holds only sound clips and a picture track none; sound
// tracks sit under the picture ones. The subtitle track, when there is
// one, holds only text clips (cues) and sits above every other track.
struct ReelTrack final {
  std::vector<Clip> clips;
  bool is_sound = false;
  bool is_subtitle = false;
  // Sound tracks only: the header's name (empty shows "A1", "A2"...),
  // Mute, Solo and the track's gain.
  QString name;
  bool is_muted = false;
  bool is_solo = false;
  double volume = 1.0;

  bool operator==(const ReelTrack&) const = default;
};

// The final video: tracks of clips, bottom to top. A higher track's
// clip is drawn over a lower one's, so a shot dropped above a
// speedpaint covers it while it plays.
struct Reel final {
  std::vector<ReelTrack> tracks =
      std::vector<ReelTrack>(static_cast<size_t>(kDefaultReelTracks));
  // Where cuts should fall, marked by ear while the song plays: sorted,
  // each frame at most once. Clips snap to them, and the stretch
  // between two is a slot a clip can be fitted into.
  std::vector<Frame> markers{};
  // The look every cue on the subtitle track shares (a cue may keep its
  // own, TextSource::is_own_look).
  ReelTextLook subtitle_look = DefaultSubtitleLook();
  // Saved text looks, by name.
  std::vector<TextStyle> text_styles{};
  // Lines pasted to be timed by tapping L: the first one becomes the
  // next cue.
  std::vector<QString> subtitle_lines{};

  static ReelTextLook DefaultSubtitleLook() {
    ReelTextLook look;
    look.letters.size = 48.0;
    look.letters.outline_width = 3.0;
    look.box = QColor(0, 0, 0, 150);
    look.place = QPointF(0.5, 0.88);
    return look;
  }

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
// True when no picture track has a clip: sound alone is not a video.
bool IsReelEmpty(const Reel& reel);
bool IsSoundClip(const Clip& clip);
bool IsTextClip(const Clip& clip);
// How many of the reel's tracks are sound tracks.
int SoundTrackCount(const Reel& reel);
// The subtitle track's index, or -1 when the reel has none yet.
int SubtitleTrack(const Reel& reel);
// The picture tracks: neither sound nor subtitles.
int PictureTrackCount(const Reel& reel);
// How a text clip looks where it sits: a cue on the subtitle track
// takes the track's look unless it keeps its own. Asserts clip is text.
const ReelTextLook& LookOf(const Reel& reel, const Clip& clip);
const TextStyle* FindTextStyle(const Reel& reel, const QString& name);
// The sound tracks that play: unmuted, and soloed when any is.
bool IsTrackHeard(const Reel& reel, int track);
// The clip on track starting where clip ends, or nullptr: the two
// touch, and clip's transition plays between them.
const Clip* NextTouching(const ReelTrack& track, const Clip& clip);

}  // namespace snapper

#endif  // SNAPPER_MODEL_REEL_H_
