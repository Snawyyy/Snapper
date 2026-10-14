#ifndef SNAPPER_UI_AUDIO_PREVIEW_H_
#define SNAPPER_UI_AUDIO_PREVIEW_H_

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>
#include <QString>

namespace snapper {

// Plays one sound file at a time to listen to it (a voice sample, a
// spoken line): Toggle starts a file, or stops it when it is the one
// playing. Starting another file stops the first. Nothing here touches
// the project or the reel's playback.
class AudioPreview final : public QObject {
  Q_OBJECT

 public:
  AudioPreview();

  // Plays path from its start, or stops it when it is playing.
  void Toggle(const QString& path);
  void Stop();
  // path is the file that is playing now (false for an empty path).
  bool IsPlaying(const QString& path) const;

 signals:
  // A file started or stopped (also when it played to its end).
  void Changed();

 private:
  void FollowState(QMediaPlayer::PlaybackState state);

  QAudioOutput output_;
  QMediaPlayer player_;
  QString path_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_AUDIO_PREVIEW_H_
