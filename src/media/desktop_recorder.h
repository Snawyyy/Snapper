#ifndef SNAPPER_MEDIA_DESKTOP_RECORDER_H_
#define SNAPPER_MEDIA_DESKTOP_RECORDER_H_

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>

#include "base/error.h"
#include "media/audio_recorder.h"

namespace snapper {

// The longest sample taken from the PC's sound; a voice sample never
// needs more.
constexpr int kMaxSampleSeconds = 30;

// Records what the PC plays (the default output's monitor) into memory
// as 48 kHz stereo 16-bit PCM, until stopped or kMaxSampleSeconds have
// passed. Qt's QMediaDevices hides the monitor sources of PipeWire and
// PulseAudio, so this reads parec's raw output instead. One take at a
// time; the process is killed with the recorder.
class DesktopRecorder final : public QObject {
  Q_OBJECT

 public:
  DesktopRecorder();
  ~DesktopRecorder() override;

  // Why the PC's sound can't be recorded here; empty when it can.
  static QString WhyNoDesktop();

  Result<void> Start();
  bool IsRecording() const {
    return process_.state() != QProcess::NotRunning;
  }
  // Ends the take and hands it over; empty when nothing was heard. Also
  // collects the take of a recording that stopped at the limit.
  Take Stop();

 signals:
  // The loudest sample of the last chunk heard, 0 to 1, for a meter.
  void Level(double peak);
  // How much has been recorded.
  void Seconds(double seconds);
  // The take reached kMaxSampleSeconds and recording ended by itself;
  // Stop() still hands the take over.
  void LimitReached();

 private:
  void Read();
  void End();

  QProcess process_;
  Take take_;
  // A sample frame split across two reads.
  QByteArray rest_;
};

}  // namespace snapper

#endif  // SNAPPER_MEDIA_DESKTOP_RECORDER_H_
