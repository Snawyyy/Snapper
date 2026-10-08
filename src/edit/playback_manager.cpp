#include "edit/playback_manager.h"

#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iterator>
#include <set>
#include <utility>

#include "anim/master_timeline.h"
#include "anim/reel_timeline.h"
#include "edit/history_manager.h"
#include "edit/timeline_rows.h"

namespace snapper {
namespace {

// Ticks faster than frames so each frame shows as soon as it is due.
constexpr int kTickMs = 8;

}  // namespace

PlaybackManager::PlaybackManager(HistoryManager* history)
    : history_(history) {
  assert(history_ != nullptr);
  ticker_.setTimerType(Qt::PreciseTimer);
  ticker_.setInterval(kTickMs);
  connect(&ticker_, &QTimer::timeout, this, &PlaybackManager::Tick);
  connect(history_, &HistoryManager::Changed, this,
          &PlaybackManager::FollowProject);
  connect(&loader_, &QFutureWatcher<Result<AudioClip>>::finished, this,
          &PlaybackManager::SongLoaded);
  FollowProject();
  assert(!IsPlaying());
}

PlaybackManager::~PlaybackManager() {
  assert(history_ != nullptr);
  // A decode still running must finish before its watcher goes.
  loader_.waitForFinished();
  assert(!loader_.isRunning());
}

void PlaybackManager::Play() {
  assert(history_ != nullptr);
  assert(frame_.index() >= 0);
  const bool is_at_end = frame_ >= LastFrame();
  if (is_at_end) {
    frame_ = HasLoop() ? loop_start_ : Frame(0);
    emit FrameChanged(frame_);
  }
  player_.Play(SecondsAtFrame(frame_));
  ticker_.start();
  emit PlayingChanged(true);
}

void PlaybackManager::Pause() {
  assert(history_ != nullptr);
  const bool was_playing = IsPlaying();
  player_.Stop();
  ticker_.stop();
  assert(!IsPlaying());
  if (was_playing) {
    emit PlayingChanged(false);
  }
}

void PlaybackManager::Toggle() {
  assert(history_ != nullptr);
  const bool was_playing = IsPlaying();
  if (was_playing) {
    Pause();
  } else {
    Play();
  }
  assert(IsPlaying() != was_playing);
}

void PlaybackManager::Seek(Frame frame) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const Frame clamped = std::min(frame, LastFrame());
  const bool is_moved = clamped != frame_;
  frame_ = clamped;
  const bool was_playing = IsPlaying();
  if (was_playing) {
    player_.Play(SecondsAtFrame(frame_));
  }
  if (is_moved) {
    emit FrameChanged(frame_);
  }
}

void PlaybackManager::Step(int delta) {
  assert(history_ != nullptr);
  assert(delta >= -kMaxFrame && delta <= kMaxFrame);
  Pause();
  const bool is_scrub = step_mode_ == StepMode::kScrub;
  if (is_scrub) {
    Seek(Frame(frame_.index() + delta));
    return;
  }
  const bool is_reel = timeline_ == Timeline::kReel;
  const std::set<Frame> changes = is_reel ? ReelCuts(history_->current())
                                          : PoseChanges(history_->current());
  Frame at = frame_;
  for (int i = 0; i < std::abs(delta); ++i) {
    const bool is_forward = delta > 0;
    const auto next = changes.upper_bound(at);
    const auto before = changes.lower_bound(at);
    const bool has_next = is_forward ? next != changes.end()
                                     : before != changes.begin();
    if (!has_next) {
      // Past the last change, the end of the track is next.
      at = is_forward ? LastFrame() : Frame(0);
      break;
    }
    at = is_forward ? *next : *std::prev(before);
  }
  Seek(at);
}

void PlaybackManager::SetStepMode(StepMode mode) {
  assert(history_ != nullptr);
  const bool is_new = mode != step_mode_;
  step_mode_ = mode;
  if (is_new) {
    emit StepModeChanged(mode);
  }
  assert(step_mode_ == mode);
}

void PlaybackManager::SetTimeline(Timeline timeline) {
  assert(history_ != nullptr);
  assert(timeline == Timeline::kShots || timeline == Timeline::kReel);
  const bool is_new = timeline != timeline_;
  if (!is_new) {
    return;
  }
  Pause();
  ClearLoop();
  std::swap(frame_, parked_);
  timeline_ = timeline;
  frame_ = std::min(frame_, LastFrame());
  emit TimelineChanged(timeline_);
  emit FrameChanged(frame_);
}

void PlaybackManager::SetLoop(Frame start, Frame end) {
  assert(start.index() >= 0);
  assert(end.index() >= 0);
  const bool is_range = start < end;
  loop_start_ = is_range ? start : Frame(0);
  loop_end_ = is_range ? end : Frame(0);
}

void PlaybackManager::ClearLoop() {
  loop_start_ = Frame(0);
  loop_end_ = Frame(0);
  assert(!HasLoop());
  assert(loop_start_.index() == 0);
}

void PlaybackManager::Tick() {
  assert(history_ != nullptr);
  assert(IsPlaying());
  Frame now = FrameAtSeconds(player_.Position());
  const bool is_loop_done = HasLoop() && !(now < loop_end_);
  if (is_loop_done) {
    now = loop_start_;
    player_.Play(SecondsAtFrame(now));
  }
  const bool is_done = !HasLoop() && now >= LastFrame();
  if (is_done) {
    now = LastFrame();
    Pause();
  }
  const bool is_new = now != frame_;
  if (is_new) {
    frame_ = now;
    emit FrameChanged(frame_);
  }
}

void PlaybackManager::FollowProject() {
  assert(history_ != nullptr);
  const QString& song = history_->current().song;
  const bool is_new_song = song != song_path_;
  if (is_new_song) {
    song_path_ = song;
    song_.reset();
    song_error_.clear();
    player_.SetClip(nullptr);
    const bool has_song = !song.isEmpty();
    if (has_song) {
      loader_.setFuture(QtConcurrent::run(DecodeAudio, song));
    }
    emit SongChanged();
  }
  const bool is_past_end = frame_ > LastFrame();
  if (is_past_end) {
    Seek(LastFrame());
  }
}

void PlaybackManager::SongLoaded() {
  assert(history_ != nullptr);
  assert(loader_.isFinished());
  Result<AudioClip> loaded = loader_.result();
  // A newer song may have been picked while this one decoded.
  const bool is_stale = history_->current().song != song_path_;
  if (is_stale) {
    return;
  }
  if (loaded) {
    song_ = std::make_shared<const AudioClip>(std::move(*loaded));
    player_.SetClip(song_);
  } else {
    song_error_ = loaded.error().message;
  }
  const bool was_playing = IsPlaying();
  if (was_playing) {
    player_.Play(SecondsAtFrame(frame_));
  }
  emit SongChanged();
}

Frame PlaybackManager::LastFrame() const {
  assert(history_ != nullptr);
  const Project& project = history_->current();
  const bool is_reel = timeline_ == Timeline::kReel;
  // The reel runs at least as long as the song, so cuts can be marked
  // over the music before any clip is placed.
  const Frame song =
      song_ != nullptr ? FrameAtSeconds(song_->Seconds()) : Frame(0);
  const int total = (is_reel ? std::max(ReelLength(project), song)
                             : TotalLength(project))
                        .index();
  assert(total >= 0);
  return Frame(std::max(total - 1, 0));
}

}  // namespace snapper
