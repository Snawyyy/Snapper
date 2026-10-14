#include "edit/record_manager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include <cassert>
#include <vector>

#include "base/text.h"
#include "edit/document_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reel_manager.h"
#include "edit/sidecar_folder.h"
#include "media/wav_file.h"

namespace snapper {
namespace {

constexpr int kBeatMs = 1000;

bool IsSoundTrackOf(const Reel& reel, int track) {
  assert(track >= -1);
  assert(reel.tracks.size() < 100000);
  const bool is_track =
      track >= 0 && track < static_cast<int>(reel.tracks.size());
  return is_track && reel.tracks[static_cast<size_t>(track)].is_sound;
}

}  // namespace

RecordManager::RecordManager(HistoryManager* history,
                             DocumentManager* document,
                             PlaybackManager* playback, ReelManager* reel,
                             const QString& data_folder)
    : history_(history),
      document_(document),
      playback_(playback),
      reel_(reel),
      data_folder_(data_folder) {
  assert(history_ != nullptr && document_ != nullptr);
  assert(playback_ != nullptr && reel_ != nullptr);
  beat_.setInterval(kBeatMs);
  connect(&beat_, &QTimer::timeout, this, &RecordManager::Tick);
  connect(&recorder_, &AudioRecorder::Level, this, &RecordManager::Level);
  connect(playback_, &PlaybackManager::PlayingChanged, this,
          &RecordManager::FollowPlaying);
}

RecordManager::~RecordManager() {
  assert(history_ != nullptr);
  assert(playback_ != nullptr);
  beat_.stop();
  recorder_.Stop();
}

Result<void> RecordManager::ToggleArm(int track) {
  assert(history_ != nullptr);
  assert(track >= -1);
  const bool is_sound = IsSoundTrackOf(history_->current().reel, track);
  if (!is_sound) {
    return std::unexpected(Error{Tr("Only a sound track can be armed.")});
  }
  armed_track_ = armed_track_ == track ? -1 : track;
  emit StateChanged();
  return {};
}

QString RecordManager::WhyNoRecord() const {
  assert(playback_ != nullptr);
  assert(reel_ != nullptr);
  const bool is_busy = state_ != State::kIdle;
  if (is_busy) {
    return QString();
  }
  const bool has_input = AudioRecorder::HasInput();
  if (!has_input) {
    return Tr("No microphone or other sound input was found.");
  }
  const bool is_reel = playback_->timeline() == Timeline::kReel;
  return is_reel ? QString() : Tr("Sound is recorded on the Video tab.");
}

Result<void> RecordManager::Toggle() {
  assert(playback_ != nullptr);
  assert(beat_.interval() == kBeatMs);
  // Rec during the count-in calls the take off.
  const bool is_counting = state_ == State::kCounting;
  if (is_counting) {
    Cancel();
    return {};
  }
  const bool is_idle = state_ == State::kIdle;
  if (!is_idle) {
    const auto made = Finish();
    if (!made) {
      return std::unexpected(made.error());
    }
    return {};
  }
  const QString why_not = WhyNoRecord();
  const bool is_blocked = !why_not.isEmpty();
  if (is_blocked) {
    return std::unexpected(Error{why_not});
  }
  is_steering_ = true;
  playback_->Pause();
  is_steering_ = false;
  count_left_ = kCountInBeats;
  SetState(State::kCounting);
  emit Beat(count_left_);
  beat_.start();
  return {};
}

void RecordManager::Tick() {
  assert(state_ == State::kCounting);
  assert(count_left_ > 0);
  --count_left_;
  emit Beat(count_left_);
  const bool is_done = count_left_ == 0;
  if (is_done) {
    beat_.stop();
    Begin();
  }
}

void RecordManager::Begin() {
  assert(state_ == State::kCounting);
  assert(playback_ != nullptr);
  const auto started = recorder_.Start();
  if (!started) {
    SetState(State::kIdle);
    emit Problem(started.error().message);
    return;
  }
  take_start_ = playback_->FrameOn(Timeline::kReel);
  SetState(State::kRecording);
  is_steering_ = true;
  playback_->Play();
  is_steering_ = false;
}

void RecordManager::FollowPlaying(bool is_playing) {
  assert(playback_ != nullptr);
  assert(beat_.interval() == kBeatMs);
  // The reel stopping (Space, or its end) ends the take.
  const bool is_take_over =
      !is_playing && !is_steering_ && state_ == State::kRecording;
  if (is_take_over) {
    const auto made = Finish();
    if (!made) {
      emit Problem(made.error().message);
    }
  }
}

void RecordManager::Cancel() {
  assert(playback_ != nullptr);
  assert(beat_.interval() == kBeatMs);
  beat_.stop();
  recorder_.Stop();
  const bool was_recording = state_ == State::kRecording;
  SetState(State::kIdle);
  if (was_recording) {
    is_steering_ = true;
    playback_->Pause();
    is_steering_ = false;
  }
}

Result<ClipId> RecordManager::Finish() {
  assert(reel_ != nullptr);
  assert(state_ != State::kIdle);
  const bool was_counting = state_ == State::kCounting;
  beat_.stop();
  const Take take = recorder_.Stop();
  SetState(State::kIdle);
  is_steering_ = true;
  playback_->Pause();
  is_steering_ = false;
  if (was_counting) {
    return std::unexpected(Error{Tr("Recording stopped before it began.")});
  }
  const bool is_empty = take.samples.empty();
  if (is_empty) {
    return std::unexpected(Error{Tr("Nothing was heard from the input.")});
  }
  const QString path = FreshTakePath();
  const bool has_folder = QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  const bool is_written =
      has_folder && file.open(QIODevice::WriteOnly) &&
      file.write(WaveFile(take.rate, take.channels, take.samples)) > 0;
  file.close();
  if (!is_written) {
    return std::unexpected(Error{Tr("Could not save the take to %1.")
                                     .arg(QDir::toNativeSeparators(path))});
  }
  const bool is_armed =
      IsSoundTrackOf(history_->current().reel, armed_track_);
  auto made = reel_->AddSounds({path}, is_armed ? armed_track_ : -1,
                               take_start_);
  const bool needs_free_track = !made && is_armed;
  if (needs_free_track) {
    // The armed track has no room there: the take goes on a free one.
    made = reel_->AddSounds({path}, -1, take_start_);
  }
  if (!made) {
    return std::unexpected(made.error());
  }
  assert(!made->empty());
  emit TakeAdded(made->front());
  return made->front();
}

QString RecordManager::TakeFolder() const {
  assert(document_ != nullptr);
  assert(!data_folder_.isEmpty() || document_->HasPath());
  const bool is_saved = document_->HasPath();
  return SidecarFolder(is_saved ? document_->path() : QString(),
                       data_folder_, Tr("%1 takes"),
                       QStringLiteral("takes"));
}

QString RecordManager::FreshTakePath() const {
  const QDir folder(TakeFolder());
  assert(!folder.path().isEmpty());
  assert(kBeatMs > 0);
  return FreshNumberedPath(folder, QStringLiteral("Take"),
                           QStringLiteral("wav"));
}

void RecordManager::SetState(State state) {
  assert(beat_.interval() == kBeatMs);
  assert(state_ == State::kIdle || state_ == State::kCounting ||
         state_ == State::kRecording);
  const bool is_new = state != state_;
  state_ = state;
  if (is_new) {
    emit StateChanged();
  }
}

}  // namespace snapper
