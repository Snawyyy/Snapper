#include "media/audio_player.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

namespace snapper {
namespace {

constexpr qint64 kBytesPerFrame = kAudioChannels * sizeof(float);

QAudioFormat ClipFormat() {
  QAudioFormat format;
  format.setSampleRate(kAudioRate);
  format.setChannelCount(kAudioChannels);
  format.setSampleFormat(QAudioFormat::Float);
  assert(format.isValid());
  assert(format.bytesPerFrame() == kBytesPerFrame);
  return format;
}

}  // namespace

PcmSource::PcmSource(std::shared_ptr<const AudioClip> clip)
    : clip_(std::move(clip)) {
  assert(clip_ != nullptr);
  const bool is_open = open(QIODevice::ReadOnly);
  assert(is_open);
}

void PcmSource::Seek(double seconds) {
  assert(std::isfinite(seconds));
  assert(clip_ != nullptr);
  const auto frame = static_cast<qint64>(std::max(seconds, 0.0) * kAudioRate);
  const auto total =
      static_cast<qint64>(clip_->samples.size() * sizeof(float));
  position_ = std::clamp<qint64>(frame * kBytesPerFrame, 0, total);
}

qint64 PcmSource::bytesAvailable() const {
  assert(clip_ != nullptr);
  assert(position_ >= 0);
  const auto total =
      static_cast<qint64>(clip_->samples.size() * sizeof(float));
  return std::max<qint64>(total - position_, 0) + QIODevice::bytesAvailable();
}

qint64 PcmSource::readData(char* data, qint64 max_size) {
  assert(data != nullptr);
  assert(max_size >= 0);
  const auto total =
      static_cast<qint64>(clip_->samples.size() * sizeof(float));
  const qint64 count = std::clamp<qint64>(total - position_, 0, max_size);
  std::memcpy(data,
              reinterpret_cast<const char*>(clip_->samples.data()) + position_,
              static_cast<size_t>(count));
  position_ += count;
  return count;
}

qint64 PcmSource::writeData(const char* data, qint64 max_size) {
  assert(data != nullptr || max_size == 0);
  assert(max_size >= 0);
  return -1;
}

AudioPlayer::AudioPlayer() {
  assert(!is_playing_);
  assert(clip_ == nullptr);
}

AudioPlayer::~AudioPlayer() {
  assert(start_ >= 0.0);
  Stop();
  assert(!is_playing_);
}

void AudioPlayer::SetClip(std::shared_ptr<const AudioClip> clip) {
  assert(clip == nullptr || clip->samples.size() % kAudioChannels == 0);
  Stop();
  clip_ = std::move(clip);
  assert(!is_playing_);
}

void AudioPlayer::Play(double from_seconds) {
  assert(std::isfinite(from_seconds));
  Stop();
  start_ = std::max(from_seconds, 0.0);
  is_playing_ = true;
  silent_clock_.start();
  const bool can_sound = clip_ != nullptr && HasOutput();
  if (can_sound) {
    source_ = std::make_unique<PcmSource>(clip_);
    source_->Seek(start_);
    sink_ = std::make_unique<QAudioSink>(QMediaDevices::defaultAudioOutput(),
                                         ClipFormat());
    sink_->start(source_.get());
  }
  assert(is_playing_);
}

void AudioPlayer::Stop() {
  assert(start_ >= 0.0);
  start_ = Position();
  const bool has_sink = sink_ != nullptr;
  if (has_sink) {
    sink_->stop();
  }
  sink_.reset();
  source_.reset();
  is_playing_ = false;
  assert(sink_ == nullptr);
}

double AudioPlayer::Position() const {
  assert(start_ >= 0.0);
  assert(sink_ == nullptr || is_playing_);
  const bool is_still = !is_playing_;
  if (is_still) {
    return start_;
  }
  const bool is_sounding = sink_ != nullptr &&
                           sink_->state() != QAudio::StoppedState;
  const double played =
      is_sounding ? static_cast<double>(sink_->processedUSecs()) / 1e6
                  : static_cast<double>(silent_clock_.nsecsElapsed()) / 1e9;
  return start_ + played;
}

bool AudioPlayer::HasOutput() const {
  const QAudioDevice device = QMediaDevices::defaultAudioOutput();
  assert(start_ >= 0.0);
  const bool is_usable = !device.isNull() &&
                         device.isFormatSupported(ClipFormat());
  return is_usable;
}

}  // namespace snapper
