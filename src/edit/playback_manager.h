#ifndef SNAPPER_EDIT_PLAYBACK_MANAGER_H_
#define SNAPPER_EDIT_PLAYBACK_MANAGER_H_

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>
#include <utility>
#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "media/audio_clip.h"
#include "media/audio_player.h"
#include "model/project.h"

namespace snapper {

class HistoryManager;
class SoundManager;

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
  // The last frame of the current timeline.
  Frame LastFrame() const;
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
  // Where the reel's sound comes from; until set (or with nullptr) the
  // song alone plays on both timelines.
  void SetSounds(SoundManager* sounds);
  // What timeline sounds like from its frame 0: the reel's mix, or on
  // the shots each shot's sound from the reel (SoundManager::MixShots),
  // or the song when there is nothing else. nullptr for silence.
  std::shared_ptr<const AudioClip> SoundOn(Timeline timeline) const;
  // The song or a sound file on the reel is still being decoded.
  bool IsLoadingSound() const;
  // SoundOn(kShots), kept until the reel, the shots' lengths, the song
  // or the loaded sounds change, for the Pose tab's waveform.
  std::shared_ptr<const AudioClip> ShotsSound();
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
  // Gives the player the current timeline's sound when it changed.
  void FreshenSound();
  // The sound changed: mixed again before the next play.
  void Stale();

  HistoryManager* history_;
  SoundManager* sounds_ = nullptr;
  // The sound the player has, and whether it is out of date.
  std::shared_ptr<const AudioClip> playing_;
  bool is_stale_ = true;
  // ShotsSound's copy and what it was made from.
  std::shared_ptr<const AudioClip> shots_sound_;
  Reel shots_reel_;
  std::vector<std::pair<ShotId, Frame>> shots_lengths_;
  bool is_shots_stale_ = true;
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
