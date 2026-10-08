#ifndef SNAPPER_EDIT_PLAYBACK_MANAGER_H_
#define SNAPPER_EDIT_PLAYBACK_MANAGER_H_

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

#include "base/error.h"
#include "base/frame.h"
#include "media/audio_clip.h"
#include "media/audio_player.h"

namespace snapper {

class HistoryManager;

// What stepping (the arrow keys) moves by: one frame at a time, or to
// the next frame where the picture changes (a key, a shot's start, or
// each frame of an ease), skipping held frames.
enum class StepMode { kScrub, kAnimation };

// The two timelines the playhead can run on: the shots one after
// another (where posing happens), or the reel (the final video).
enum class Timeline { kShots, kReel };

// The playhead and playing the song under it, on either timeline; each
// timeline keeps its own place. The song's clock drives the picture, so
// frames never drift from the music. The song is decoded in the
// background whenever the project's song changes.
class PlaybackManager final : public QObject {
  Q_OBJECT

 public:
  explicit PlaybackManager(HistoryManager* history);
  ~PlaybackManager() override;

  // The playhead on the current timeline.
  Frame frame() const { return frame_; }
  Timeline timeline() const { return timeline_; }
  // The playhead's place on timeline, current or not.
  Frame FrameOn(Timeline timeline) const {
    return timeline == timeline_ ? frame_ : parked_;
  }
  // Stops playing and moves the playhead to timeline, where it was last.
  // The loop is dropped: it belonged to the other timeline.
  void SetTimeline(Timeline timeline);
  bool IsPlaying() const { return player_.IsPlaying(); }

  void Play();
  void Pause();
  void Toggle();
  // Clamped to the current timeline; the reel lasts at least as long as
  // the song.
  void Seek(Frame frame);
  // delta frames, or in animation mode delta pose changes (shots) or
  // clip cuts (reel).
  void Step(int delta);
  StepMode step_mode() const { return step_mode_; }
  void SetStepMode(StepMode mode);
  // Playing wraps from end back to start.
  void SetLoop(Frame start, Frame end);
  void ClearLoop();
  bool HasLoop() const { return loop_end_.index() > loop_start_.index(); }
  Frame loop_start() const { return loop_start_; }
  Frame loop_end() const { return loop_end_; }

  // The decoded song for the waveform; nullptr while there is none.
  std::shared_ptr<const AudioClip> song() const { return song_; }
  // Why the song couldn't be loaded; empty when it could.
  const QString& song_error() const { return song_error_; }
  bool IsLoadingSong() const { return loader_.isRunning(); }

 signals:
  void FrameChanged(Frame frame);
  void PlayingChanged(bool is_playing);
  void SongChanged();
  void StepModeChanged(StepMode mode);
  void TimelineChanged(Timeline timeline);

 private:
  void Tick();
  void FollowProject();
  void SongLoaded();
  Frame LastFrame() const;

  HistoryManager* history_;
  AudioPlayer player_;
  QTimer ticker_;
  QFutureWatcher<Result<AudioClip>> loader_;
  QString song_path_;
  std::shared_ptr<const AudioClip> song_;
  QString song_error_;
  Frame frame_;
  // The other timeline's playhead.
  Frame parked_;
  Timeline timeline_ = Timeline::kShots;
  Frame loop_start_;
  Frame loop_end_;
  StepMode step_mode_ = StepMode::kScrub;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_PLAYBACK_MANAGER_H_
