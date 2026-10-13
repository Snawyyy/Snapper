#ifndef SNAPPER_EDIT_REEL_MANAGER_H_
#define SNAPPER_EDIT_REEL_MANAGER_H_

#include <QPointF>
#include <QString>
#include <QStringList>

#include <vector>

#include "base/error.h"
#include "anim/reel_timeline.h"
#include "base/frame.h"
#include "edit/reel_edits.h"
#include "io/subtitle_file.h"
#include "model/reel.h"
#include "model/shot.h"

namespace snapper {

class HistoryManager;

QString TransitionName(TransitionKind kind);

// What a shot clip's edge drag does to its shot's keys: Hold keeps the
// keys where they are (new frames hold the last pose), Stretch moves
// every key in proportion so the same action plays faster or slower.
enum class EdgeMode { kHold, kStretch };

// Where a shot resize ended up: how many frames each shot grew (less
// than asked when something stopped it, and why), and the grabbed
// clip's shot's new length.
struct ShotResize final {
  int delta = 0;
  Frame length;
  QString stopped;
};

// A new text clip's length, a cue's longest when tapped, and how long L
// must be held to end a cue on letting go.
constexpr int kTextClipFrames = 2 * kFramesPerSecond;
constexpr int kCueFrames = 4 * kFramesPerSecond;
constexpr int kHoldFrames = kFramesPerSecond / 4;
// A new shot's length where nothing after it bounds the free space.
constexpr int kNewShotFrames = 2 * kFramesPerSecond;

// The usual places for text: bottom middle, top middle, the lower third
// at the left, the middle.
enum class TextSpot { kBottom, kTop, kLowerThird, kMiddle };
constexpr int kTextSpotCount = 4;

// Where a text drag put the text, and whether it caught on a guide
// across (x) or down (y), for the preview to show the guide.
struct TextPlaced final {
  QPointF place;
  bool is_snapped_x = false;
  bool is_snapped_y = false;
};

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
  // New shot here (edit/reel_new_shot.cpp). A new blank shot exactly
  // to - from frames long, laid on track over that stretch, as one step:
  // for a gap between markers or a marked range. track -1 takes the
  // lowest picture track with room there. Returns its clip.
  Result<ClipId> AddNewShot(int track, Frame from, Frame to);
  // Why AddNewShot can't act there; empty when it can.
  QString WhyNoNewShot(int track, Frame from, Frame to) const;
  // The stretch a new shot at at on track fills: the gap between the cut
  // markers around at (cut back to the free space), else the free space
  // between the clips around at, or kNewShotFrames from at when no clip
  // follows. Invalid when at is on a clip. For track -1, the gap around
  // at, or kNewShotFrames from at.
  Slot NewShotSpan(int track, Frame at) const;
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
  // Only an empty track goes, and the reel keeps at least one of each
  // kind.
  Result<void> RemoveTrack(int track);

  // Sound (edit/reel_sound.cpp). A sound file as a clip source, its
  // length read now.
  Result<SoundSource> ReadSound(const QString& path) const;
  // Sound files laid in a row from start, as one step: on track when it
  // is a sound track, else on the first sound track with room for the
  // whole row (a new one when none has room and one can be added).
  Result<std::vector<ClipId>> AddSounds(const QStringList& paths, int track,
                                        Frame start);
  // A new empty sound track on top of the others.
  Result<void> AddSoundTrack();
  // gain 0 (silent) to kMaxGain (+6 dB), for the clip's volume line.
  Result<void> SetClipVolume(ClipId clip, double gain);
  // Frames the clip fades in or out over, kept inside the clip.
  Result<void> SetFadeIn(ClipId clip, Frame frames);
  Result<void> SetFadeOut(ClipId clip, Frame frames);
  Result<void> SetTrackVolume(int track, double gain);
  Result<void> SetTrackMuted(int track, bool is_muted);
  Result<void> SetTrackSolo(int track, bool is_solo);
  // A reel with no sound tracks (a new project, or a file from before
  // them) gets the default ones, and the project's song becomes a clip
  // from frame 0 on the first, so its timing does not change. Not an
  // undo step: for a project just opened or made.
  static void AddSoundTracks(Project* project);
  // Choosing another song: the clips of old_song go and the project's
  // song (when it has one) is laid from frame 0 on the first sound
  // track with room. Part of the song's own step.
  static void ReplaceSong(Project* project, const QString& old_song);

  // Text clips (edit/reel_text.cpp). A text clip at at, kTextClipFrames
  // long, on the highest picture track with room there (a new one on
  // top when none has and one can be added).
  Result<ClipId> AddText(Frame at);
  QString WhyNoAddText(Frame at) const;
  // The words of a text clip or cue, as one step.
  Result<void> SetText(ClipId clip, const QString& text);
  // The lettering and box of every text clip in clips from look, each
  // keeping its own place; a cue sharing the subtitle track's look
  // changes the track's look. Unlinks them from their style. label
  // names the step ("Text size").
  Result<void> SetTextLook(const std::vector<ClipId>& clips,
                           const ReelTextLook& look, const QString& label);
  // Drags clip's text: its middle to place (a share of the frame) and
  // its letters to size, from where the open drag started. Within reach
  // (a share of the frame; 0 for none) the middle snaps to the frame's
  // middle and the text's edges to the title-safe margin.
  Result<TextPlaced> PlaceText(ClipId clip, QPointF place, double size,
                               double reach);
  // Puts each text clip's text at a usual place, inside the title-safe
  // margin.
  Result<void> SetTextSpot(const std::vector<ClipId>& clips, TextSpot spot);
  // How text clips appear (is_appear) or leave, over length frames.
  Result<void> SetTextMotion(const std::vector<ClipId>& clips, bool is_appear,
                             TextMotion motion, Frame length);
  // Saves from's look as the style name (replacing one so named), links
  // from to it, and every clip linked to it takes the new look, keeping
  // its place.
  Result<void> SaveTextStyle(const QString& name, ClipId from);
  // Gives clips the look of the style name, keeping their places; with
  // is_linked they follow later changes to it.
  Result<void> ApplyTextStyle(const std::vector<ClipId>& clips,
                              const QString& name, bool is_linked);
  // Takes the style away; clips linked to it keep their look.
  Result<void> RemoveTextStyle(const QString& name);
  // Cues: keep their own look (true) or share the track's.
  Result<void> SetOwnLook(const std::vector<ClipId>& clips, bool is_own);

  // Subtitles (edit/reel_subtitles.cpp). The subtitle track's shared
  // look.
  Result<void> SetSubtitleLook(const ReelTextLook& look, const QString& label);
  // The lines waiting to be timed: every non-empty line of text.
  Result<void> SetSubtitleLines(const QString& text);
  // L: ends the cue running at at and starts the next waiting line
  // there, up to kCueFrames long or until the next cue. Makes the
  // subtitle track when there is none.
  Result<ClipId> TapCue(Frame at);
  QString WhyNoTap(Frame at) const;
  // L let go after held frames: a hold (kHoldFrames or more) ends the
  // cue the last tap started at at. A short tap changes nothing.
  Result<void> ReleaseCue(Frame at, Frame held);
  // Alt drag of a cue's end: it and the next cue's start, when the two
  // touch, move to end together from where the open drag started, so no
  // gap opens.
  Result<void> RollCueEdge(ClipId clip, Frame end);
  // The same for a cue's start and the end of the cue touching it.
  Result<void> RollCueStart(ClipId clip, Frame start);
  // True when an edge drag of clip with is_alt rolls it with the cue it
  // touches: an Alt drag of a cue.
  bool IsCueRoll(ClipId clip, bool is_alt) const;
  // An SRT or WebVTT file's cues onto the subtitle track (made when
  // there is none), replacing the cues they overlap.
  Result<std::vector<ClipId>> ImportSubtitles(const QString& path);
  // Every cue's text with find replaced by with, as one step; how many
  // cues changed.
  Result<int> ReplaceInCues(const QString& find, const QString& with);
  // The subtitle track's cues, in order, for the list and the files.
  static std::vector<SubtitleCue> CuesOf(const Reel& reel);

  // How clip hands over to the clip touching its end on its track:
  // a cut, or a mix 1 to LongestTransition frames long.
  Result<void> SetTransition(ClipId clip, Transition transition);

  // Drops a cut marker at at, or takes away the one already there.
  Result<void> ToggleMarker(Frame at);

  // Resizing shots from the reel (edit/reel_resize.cpp).
  // True when clip shows its whole shot, from its first frame to its
  // last.
  bool IsFullClip(ClipId clip) const;
  // True when dragging clip's edge changes its shot's length rather than
  // trimming the clip: a full shot clip, or with alt a partial one.
  bool IsShotResize(ClipId clip, bool is_alt) const;
  // The undo name of resizing clip's shot with picked: "Resize shot
  // Intro", or "Resize shots" when picked holds clips of other shots.
  QString ResizeLabel(const std::vector<ClipId>& picked, ClipId clip) const;
  // Drags clip's start or end edge to edge, from where the open drag
  // started (HistoryManager::before), changing the length of its shot
  // and of every other picked shot clip's shot by the same frames, by
  // edge_mode. Full clips of those shots follow; the dragged ones' edges
  // move; partial ones keep their in point and length. With ripple,
  // later clips on their tracks move by the change and a start edge
  // stays put. Stops short at a clip in the way or the length limits.
  Result<ShotResize> ResizeShots(const std::vector<ClipId>& picked,
                                 ClipId clip, bool is_start, Frame edge,
                                 bool is_ripple);
  // Why clip's shot can't be fitted to the gap between the cut markers
  // around at; empty when it can.
  QString WhyNoFit(ClipId clip, Frame at) const;
  // Makes clip's shot exactly as long as that gap (held or stretched by
  // mode) and lays clip over it, as one step.
  Result<void> FitShotToGap(ClipId clip, Frame at, EdgeMode mode);
  EdgeMode edge_mode() const { return edge_mode_; }
  void SetEdgeMode(EdgeMode mode) { edge_mode_ = mode; }

  // Why each can't act now, for greyed-out controls; empty when it can.
  QString WhyNoSplit(const std::vector<ClipId>& clips, Frame at) const;
  // For copy, cut, remove and ripple delete: something is picked.
  QString WhyNoPick(const std::vector<ClipId>& clips) const;
  QString WhyNoPaste(Frame at) const;
  QString WhyNoDuplicate(const std::vector<ClipId>& clips) const;
  QString WhyNoTransition(ClipId clip) const;
  QString WhyNoAddTrack() const;
  QString WhyNoAddSoundTrack() const;
  QString WhyNoRemoveTrack(int track) const;

 private:
  HistoryManager* history_;
  // Copied clips, starts counted from the first one's.
  std::vector<Lifted> clipboard_;
  // How shot clip edge drags treat the keys; not saved, not undone.
  EdgeMode edge_mode_ = EdgeMode::kHold;
  // The cue the last L press started, which a hold of L ends.
  ClipId last_cue_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_REEL_MANAGER_H_
