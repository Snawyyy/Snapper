#ifndef SNAPPER_EDIT_RECORD_MANAGER_H_
#define SNAPPER_EDIT_RECORD_MANAGER_H_

#include <QObject>
#include <QString>
#include <QTimer>

#include "base/error.h"
#include "base/frame.h"
#include "media/audio_recorder.h"
#include "model/reel.h"

namespace snapper {

class DocumentManager;
class HistoryManager;
class PlaybackManager;
class ReelManager;

// The count-in before a take: this many beats, one a second.
constexpr int kCountInBeats = 3;

// Recording sound onto the reel, to plan timing by mouthing a scene's
// noises: arm a sound track, then Record counts in and records from the
// reel's playhead while the reel plays; Record again (or the reel
// stopping) ends the take. Each take is saved as a WAV file beside the
// project (in "<project name> takes"), or in the app's data folder for
// a project never saved, and laid on the armed track from where it
// started (any free sound track when none is armed or the armed one has
// no room), as one undo step. Escape during a take throws it away.
class RecordManager final : public QObject {
  Q_OBJECT

 public:
  enum class State { kIdle, kCounting, kRecording };

  RecordManager(HistoryManager* history, DocumentManager* document,
                PlaybackManager* playback, ReelManager* reel,
                const QString& data_folder);
  ~RecordManager() override;

  State state() const { return state_; }
  // Beats left in the count-in.
  int count_left() const { return count_left_; }
  // The sound track takes go to; -1 for none (any free one).
  int armed_track() const { return armed_track_; }
  // Arms track, or disarms it when it is the armed one. Not saved, not
  // an undo step.
  Result<void> ToggleArm(int track);
  // Why Record can't start now; empty when it can.
  QString WhyNoRecord() const;
  // Record: starts the count-in when idle, calls it off while counting,
  // ends the take while recording.
  Result<void> Toggle();
  // Escape: drops the count-in or the take, keeping nothing.
  void Cancel();
  // Where takes of the open project are saved.
  QString TakeFolder() const;
  // The reel frame the take started at.
  Frame take_start() const { return take_start_; }

 signals:
  void StateChanged();
  // Each beat of the count-in, with the beats left (0 when it starts).
  void Beat(int left);
  // The loudest sample just heard, 0 to 1.
  void Level(double peak);
  // A take landed on the reel as clip.
  void TakeAdded(ClipId clip);
  void Problem(const QString& why);

 private:
  void Tick();
  void Begin();
  Result<ClipId> Finish();
  void SetState(State state);
  // A take file name not used yet in the take folder.
  QString FreshTakePath() const;
  void FollowPlaying(bool is_playing);

  HistoryManager* history_;
  DocumentManager* document_;
  PlaybackManager* playback_;
  ReelManager* reel_;
  QString data_folder_;
  AudioRecorder recorder_;
  QTimer beat_;
  State state_ = State::kIdle;
  int count_left_ = 0;
  int armed_track_ = -1;
  Frame take_start_;
  // True while this manager starts or stops playback itself.
  bool is_steering_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_RECORD_MANAGER_H_
