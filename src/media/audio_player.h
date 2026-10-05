#ifndef SNAPPER_MEDIA_AUDIO_PLAYER_H_
#define SNAPPER_MEDIA_AUDIO_PLAYER_H_

#include <QAudioSink>
#include <QElapsedTimer>
#include <QIODevice>
#include <QObject>

#include <memory>

#include "media/audio_clip.h"

namespace snapper {

// Feeds a clip's samples to the sound card from a start point.
class PcmSource final : public QIODevice {
  Q_OBJECT

 public:
  explicit PcmSource(std::shared_ptr<const AudioClip> clip);

  // Next read starts at seconds into the clip.
  void Seek(double seconds);
  bool isSequential() const override { return true; }
  qint64 bytesAvailable() const override;

 protected:
  qint64 readData(char* data, qint64 max_size) override;
  qint64 writeData(const char* data, qint64 max_size) override;

 private:
  std::shared_ptr<const AudioClip> clip_;
  qint64 position_ = 0;
};

// The playback clock. With a song and a sound card, time is how much
// sound has played, so picture follows the music exactly. Without
// either it is plain elapsed time, so playback still works silently.
class AudioPlayer final : public QObject {
  Q_OBJECT

 public:
  AudioPlayer();
  ~AudioPlayer() override;

  // nullptr plays silence.
  void SetClip(std::shared_ptr<const AudioClip> clip);
  void Play(double from_seconds);
  void Stop();
  bool IsPlaying() const { return is_playing_; }
  // Where playback is now, in seconds from the clip's start.
  double Position() const;
  // False when there is no sound card, so the clock is silent.
  bool HasOutput() const;

 private:
  std::shared_ptr<const AudioClip> clip_;
  std::unique_ptr<PcmSource> source_;
  std::unique_ptr<QAudioSink> sink_;
  QElapsedTimer silent_clock_;
  double start_ = 0.0;
  bool is_playing_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_MEDIA_AUDIO_PLAYER_H_
