#include "ui/audio_preview.h"

#include <QUrl>

#include <cassert>

namespace snapper {

AudioPreview::AudioPreview() {
  assert(path_.isEmpty());
  assert(player_.source().isEmpty());
  player_.setAudioOutput(&output_);
  connect(&player_, &QMediaPlayer::playbackStateChanged, this,
          &AudioPreview::FollowState);
}

void AudioPreview::Toggle(const QString& path) {
  assert(!path.isEmpty());
  assert(player_.audioOutput() == &output_);
  const bool is_playing = IsPlaying(path);
  if (is_playing) {
    Stop();
    return;
  }
  PlayFrom(path, 0.0);
}

void AudioPreview::PlayFrom(const QString& path, double seconds) {
  assert(!path.isEmpty());
  assert(seconds >= 0.0);
  // Stopped first, so the stop it reports is not taken for the new file's.
  player_.stop();
  path_ = path;
  const bool is_new_file = player_.source() != QUrl::fromLocalFile(path);
  if (is_new_file) {
    player_.setSource(QUrl::fromLocalFile(path));
  }
  player_.setPosition(static_cast<qint64>(seconds * 1000.0));
  player_.play();
  emit Changed();
}

double AudioPreview::Seconds() const {
  assert(player_.audioOutput() == &output_);
  assert(player_.position() >= 0);
  return static_cast<double>(player_.position()) / 1000.0;
}

void AudioPreview::Stop() {
  assert(player_.audioOutput() == &output_);
  assert(path_.size() < 100000);
  player_.stop();
  path_.clear();
  emit Changed();
}

bool AudioPreview::IsPlaying(const QString& path) const {
  assert(path.size() < 100000);
  assert(player_.audioOutput() == &output_);
  const bool is_file = path == path_;
  return is_file && player_.playbackState() == QMediaPlayer::PlayingState;
}

void AudioPreview::FollowState(QMediaPlayer::PlaybackState state) {
  assert(player_.audioOutput() == &output_);
  assert(path_.size() < 100000);
  const bool is_over = state == QMediaPlayer::StoppedState;
  if (is_over) {
    path_.clear();
  }
  emit Changed();
}

}  // namespace snapper
