#ifndef SNAPPER_MEDIA_AUDIO_RECORDER_H_
#define SNAPPER_MEDIA_AUDIO_RECORDER_H_

#include <QAudioSource>
#include <QByteArray>
#include <QIODevice>
#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <vector>

#include "base/error.h"

namespace snapper {

// What a recording heard: 16-bit samples interleaved by channel.
struct Take final {
  int rate = 0;
  int channels = 0;
  std::vector<std::int16_t> samples;

  double Seconds() const {
    return rate > 0 && channels > 0
               ? static_cast<double>(samples.size()) / channels / rate
               : 0.0;
  }
};

// Records the default sound input (a microphone) into memory, as 16-bit
// PCM, until stopped. One take at a time; the longest is
// kMaxAudioSeconds, after which it keeps only what it has.
class AudioRecorder final : public QObject {
  Q_OBJECT

 public:
  AudioRecorder();
  ~AudioRecorder() override;
  AudioRecorder(const AudioRecorder&) = delete;
  AudioRecorder& operator=(const AudioRecorder&) = delete;

  // A sound input exists to record from.
  static bool HasInput();
  // The input's name, for tooltips; empty when there is none.
  static QString InputName();

  Result<void> Start();
  bool IsRecording() const { return source_ != nullptr; }
  // Ends the take and hands it over; empty when nothing was recorded.
  Take Stop();

 signals:
  // The loudest sample of the last chunk heard, 0 to 1, for a meter.
  void Level(double peak);

 private:
  void Read();

  std::unique_ptr<QAudioSource> source_;
  // Owned by source_.
  QIODevice* device_ = nullptr;
  Take take_;
  // A sample split across two reads.
  QByteArray rest_;
};

}  // namespace snapper

#endif  // SNAPPER_MEDIA_AUDIO_RECORDER_H_
