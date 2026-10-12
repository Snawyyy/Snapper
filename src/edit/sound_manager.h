#ifndef SNAPPER_EDIT_SOUND_MANAGER_H_
#define SNAPPER_EDIT_SOUND_MANAGER_H_

#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include <map>
#include <memory>
#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "media/audio_clip.h"
#include "model/project.h"

namespace snapper {

class HistoryManager;

// Waveform resolution: the loudest sample of each slice this long.
constexpr int kPeaksPerSecond = 200;
// The quietest a volume line shows before silence.
constexpr double kQuietestDb = -40.0;

// True for the sound files the reel takes by name: WAV, MP3, OGG,
// FLAC, and M4A, AAC, Opus.
bool IsSoundFile(const QString& path);
// gain as decibels; -infinity for silence.
double GainToDb(double gain);
// "+2.5 dB", "-12.0 dB" or "Silent", for the value chip.
QString GainText(double gain);
// Where gain sits on a volume line, 0 (silent, at the bottom) to 1
// (+6 dB, at the top), even in decibels from kQuietestDb up.
double GainToLevel(double gain);
double LevelToGain(double level);

// The reel's sound: every sound file on it decoded once in the
// background and kept, with its waveform; the mix of the heard sound
// tracks; and which reel clip a shot borrows its sound from on the
// Pose tab. It never edits the project: ReelManager does.
class SoundManager final : public QObject {
  Q_OBJECT

 public:
  explicit SoundManager(HistoryManager* history);
  ~SoundManager() override;

  // The decoded file; nullptr while it loads or when it can't be read.
  std::shared_ptr<const AudioClip> Sound(const QString& path) const;
  // Why path can't be read; empty while it loads or when it can.
  QString ErrorOf(const QString& path) const;
  bool IsLoading() const;
  // The loudest sample, 0 to 1, between two times in the file, from the
  // waveform worked out once when it loaded; 0 while it loads.
  float PeakBetween(const QString& path, double from, double to) const;

  // The heard sound tracks mixed from reel frame 0 for length frames:
  // each clip's volume and fades, each track's volume. Files still
  // loading are silent.
  AudioClip MixReel(const Project& project, Frame length) const;
  // The reel clip a shot plays the sound under on the Pose tab: the one
  // last picked or played on the Video tab when it still shows the
  // shot, else the first on the reel; invalid when it is on no clip.
  ClipId SoundClipOf(const Project& project, ShotId shot) const;
  // Every clip showing shot, earliest first, for picking another.
  std::vector<ClipId> ClipsOf(const Project& project, ShotId shot) const;
  // The shot of clip now borrows its sound from clip.
  void PreferClip(ClipId clip);
  // What the shots timeline plays, one shot after another: a shot on
  // the reel gets the reel's mix under its clip (frames the clip leaves
  // out are silent), any other the song where the shot sits, as before
  // there was a reel. nullptr when no shot is on the reel: the song
  // alone is the sound.
  std::shared_ptr<const AudioClip> MixShots(
      const Project& project, const AudioClip* song) const;

 signals:
  // A file finished loading, or the borrowed clip changed: the mix and
  // the waveforms are different.
  void SoundsChanged();

 private:
  struct Entry final {
    std::shared_ptr<const AudioClip> clip;
    std::vector<float> peaks;
    QString error;
    std::unique_ptr<QFutureWatcher<Result<AudioClip>>> loader;
  };

  // Starts loading every sound file on the reel not yet loaded.
  void Follow();
  void Loaded(const QString& path);

  HistoryManager* history_;
  std::map<QString, Entry> sounds_;
  // By shot: the clip picked last.
  std::map<ShotId, ClipId> preferred_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_SOUND_MANAGER_H_
