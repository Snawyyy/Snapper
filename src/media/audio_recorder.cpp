#include "media/audio_recorder.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

#include "base/text.h"
#include "media/audio_clip.h"

namespace snapper {
namespace {

constexpr int kRecordRate = 48000;
constexpr int kRecordChannels = 1;
constexpr double kShortMax = 32768.0;

QAudioFormat RecordFormat() {
  QAudioFormat format;
  format.setSampleRate(kRecordRate);
  format.setChannelCount(kRecordChannels);
  format.setSampleFormat(QAudioFormat::Int16);
  assert(format.isValid());
  assert(format.bytesPerSample() == 2);
  return format;
}

}  // namespace

AudioRecorder::AudioRecorder() {
  assert(source_ == nullptr);
  assert(take_.samples.empty());
}

AudioRecorder::~AudioRecorder() {
  assert(kRecordRate > 0);
  const bool is_running = source_ != nullptr;
  if (is_running) {
    source_->stop();
  }
}

bool AudioRecorder::HasInput() {
  assert(kRecordChannels > 0);
  const bool has_input = !QMediaDevices::defaultAudioInput().isNull();
  return has_input;
}

QString AudioRecorder::InputName() {
  assert(kRecordRate > 0);
  const QAudioDevice input = QMediaDevices::defaultAudioInput();
  return input.isNull() ? QString() : input.description();
}

Result<void> AudioRecorder::Start() {
  assert(kRecordChannels > 0);
  const bool is_busy = source_ != nullptr;
  if (is_busy) {
    return std::unexpected(Error{Tr("A take is already recording.")});
  }
  const QAudioDevice input = QMediaDevices::defaultAudioInput();
  const bool is_missing = input.isNull();
  if (is_missing) {
    return std::unexpected(
        Error{Tr("No microphone or other sound input was found.")});
  }
  QAudioFormat format = RecordFormat();
  const bool is_supported = input.isFormatSupported(format);
  if (!is_supported) {
    return std::unexpected(Error{
        Tr("The sound input can't record 48 kHz 16-bit sound.")});
  }
  take_ = Take{kRecordRate, kRecordChannels, {}};
  rest_.clear();
  source_ = std::make_unique<QAudioSource>(input, format);
  device_ = source_->start();
  const bool is_open = device_ != nullptr;
  if (!is_open) {
    source_.reset();
    return std::unexpected(Error{Tr("The sound input could not be opened.")});
  }
  connect(device_, &QIODevice::readyRead, this, &AudioRecorder::Read);
  return {};
}

void AudioRecorder::Read() {
  assert(device_ != nullptr);
  assert(take_.channels > 0);
  rest_.append(device_->readAll());
  const qsizetype whole = rest_.size() / 2 * 2;
  const size_t count = static_cast<size_t>(whole / 2);
  const size_t room =
      static_cast<size_t>(kMaxAudioSeconds) * kRecordRate * kRecordChannels;
  const size_t kept = std::min(count, room - std::min(room, take_.samples.size()));
  const size_t old = take_.samples.size();
  take_.samples.resize(old + kept);
  std::memcpy(take_.samples.data() + old, rest_.constData(), kept * 2);
  rest_.remove(0, whole);
  int peak = 0;
  for (size_t i = old; i < old + kept; ++i) {
    peak = std::max(peak, std::abs(static_cast<int>(take_.samples[i])));
  }
  emit Level(peak / kShortMax);
}

Take AudioRecorder::Stop() {
  assert(take_.rate >= 0);
  const bool is_running = source_ != nullptr;
  if (!is_running) {
    return Take{};
  }
  Read();
  source_->stop();
  source_.reset();
  device_ = nullptr;
  Take done = std::move(take_);
  take_ = Take{};
  return done;
}

}  // namespace snapper
