#ifndef SNAPPER_MEDIA_VOICE_WORKER_H_
#define SNAPPER_MEDIA_VOICE_WORKER_H_

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include "base/error.h"

namespace snapper {

enum class VoiceKind { kClone, kDesign, kTranscribe };

// One job for the voice worker. A clone job reads sample (and words,
// what the sample says); a design job reads describe; both write the
// speech to out as a WAV file. A transcribe job reads only sample and
// answers with the words heard in it.
struct VoiceJob final {
  VoiceKind kind = VoiceKind::kClone;
  QString text;
  QString language;
  QString sample;
  QString words;
  QString describe;
  QString out;
};

// The long-lived Python process that speaks lines (tools/voice_gen), and
// the JSON-lines talk with it: a job per line in, an answer per line
// out. It starts on the first job, since its models take a while to
// load, and stays up. One job at a time. A worker that dies is started
// again by the next job; one that dies mid-job fails that job with the
// last lines it wrote to its error output.
class VoiceWorker final : public QObject {
  Q_OBJECT

 public:
  VoiceWorker(QString python, QString script);
  ~VoiceWorker() override;

  // Python and the script both exist.
  bool IsInstalled() const;
  bool IsBusy() const { return pending_id_ != 0; }
  // The process is up, so its models may already be loaded.
  bool IsRunning() const {
    return process_.state() != QProcess::NotRunning;
  }
  // Sends job; the answers come as the signals below.
  Result<void> Submit(const VoiceJob& job);
  // Kills the worker and drops the job; the next job starts it afresh.
  void Cancel();

 signals:
  // The worker is loading a model for the job (its first of that kind).
  void Loading();
  void Speaking();
  // A line spoken (seconds long), or a sample transcribed (text).
  void Done(double seconds, const QString& text);
  void Failed(const QString& why);

 private:
  void ReadAnswers();
  void ReadErrors();
  void Answer(const QByteArray& line);
  void Finished();
  void Stop();

  QString python_;
  QString script_;
  QProcess process_;
  QByteArray answers_;
  // The last lines the worker wrote to its error output.
  QStringList recent_errors_;
  int next_id_ = 1;
  // The job being spoken; 0 for none.
  int pending_id_ = 0;
  // True while Cancel ends the process, so it is not a crash.
  bool is_cancelling_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_MEDIA_VOICE_WORKER_H_
