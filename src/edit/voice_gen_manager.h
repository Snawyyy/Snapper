#ifndef SNAPPER_EDIT_VOICE_GEN_MANAGER_H_
#define SNAPPER_EDIT_VOICE_GEN_MANAGER_H_

#include <QObject>
#include <QString>
#include <QTemporaryDir>

#include "base/error.h"
#include "media/desktop_recorder.h"
#include "media/voice_worker.h"
#include "model/reel.h"

namespace snapper {

class DocumentManager;
class PlaybackManager;
class ReelManager;
class VoiceLibrary;

// Where setup.sh puts the Python of engine, under the data folder; each
// engine has its own, as their libraries clash.
QString VoicePython(const QString& data_folder, VoiceEngine engine);
// The engine's name as the panel shows it.
QString VoiceEngineName(VoiceEngine engine);

// Speaking lines in a cloned or designed voice, for the Voice panel on
// the Video tab. The speaking is done by tools/voice_gen/voice_gen.py in
// its own Python (installed by tools/voice_gen/setup.sh), started on the
// first line and kept up, since its models take a while to load. This
// manager checks what a line needs, keeps the last spoken line (in a
// temporary folder, a fresh file per line, so a player never holds a
// file being overwritten), takes a sample from the PC's sound, and lays
// a spoken line on the reel at the playhead as one undo step. Nothing
// here is part of the project until a line is added to the reel.
class VoiceGenManager final : public QObject {
  Q_OBJECT

 public:
  // kListening: transcribing a sample.
  enum class State { kIdle, kLoading, kSpeaking, kListening };

  VoiceGenManager(DocumentManager* document, PlaybackManager* playback,
                  ReelManager* reel, VoiceLibrary* voices,
                  const QString& data_folder);
  ~VoiceGenManager() override;

  State state() const { return state_; }
  bool IsBusy() const { return state_ != State::kIdle; }

  // Why a line can't be spoken now; empty when it can.
  QString WhyNoSpeakClone(const QString& sample_path, const QString& text,
                          VoiceEngine engine, const QString& language) const;
  // Why engine can't be used: its Python is not installed.
  QString WhyNoEngine(VoiceEngine engine) const;
  // Why the emotion can't be set with engine (Qwen copies the sample's).
  QString WhyNoEmotion(VoiceEngine engine) const;
  QString WhyNoSpeakDesign(const QString& describe,
                           const QString& text) const;
  // Says text in the voice of the sample with engine; words is what the
  // sample says (Qwen without them clones from its sound alone; Chatterbox
  // never reads them). language is "Auto" or a name; emotion is
  // Chatterbox's, kMinEmotion to kMaxEmotion.
  Result<void> SpeakClone(const QString& sample_path, const QString& words,
                          const QString& text, const QString& language,
                          VoiceEngine engine, double emotion);
  // Says text in a voice made from a description.
  Result<void> SpeakDesign(const QString& describe, const QString& text,
                           const QString& language);
  // Drops the line being spoken. The worker is killed, so the next line
  // loads its model again.
  void Cancel();

  // The last line spoken: its file, length and the voice's name ("Designed
  // voice", the saved voice's name, or "Sample"). Empty before the first.
  const QString& last_take() const { return last_take_; }
  double last_seconds() const { return last_seconds_; }
  QString last_voice_name() const;
  bool IsLastDesigned() const {
    return take_job_.kind == VoiceKind::kDesign;
  }

  // Why the sample can't be transcribed now; empty when it can.
  QString WhyNoTranscribe(const QString& sample_path) const;
  // Writes down what is said in the sample (Whisper, in the same
  // worker); the words arrive as Transcribed.
  Result<void> Transcribe(const QString& sample_path);

  // Why the last line can't be added to the reel; empty when it can.
  QString WhyNoAdd() const;
  // Copies the last line into "<project name> voices" beside the saved
  // project (the data folder's voices_used for a project never saved) and
  // lays it on the reel at the playhead, as one undo step.
  Result<ClipId> AddToReel();

  // Why the last designed line can't be saved as a voice.
  QString WhyNoSaveDesigned() const;
  // Saves the last designed line as a voice that clones from it; the
  // line it said becomes the words, the description is kept.
  Result<void> SaveDesigned(const QString& name, bool replace);

  // Why a sample can't be taken from the PC's sound now; empty when it
  // can (also while one is being taken, so it can be ended).
  QString WhyNoCapture() const;
  bool IsCapturing() const { return recorder_.IsRecording(); }
  // Starts taking a sample from the PC's sound, or ends it; the sample
  // arrives as SampleCaptured.
  Result<void> ToggleCapture();
  // How long a sample file lasts.
  Result<double> SampleSeconds(const QString& path) const;

 signals:
  void StateChanged();
  // The last line is spoken and waits in last_take().
  void TakeReady();
  // The loudest sound just heard while taking a sample, 0 to 1.
  void CaptureLevel(double peak);
  void CaptureSeconds(double seconds);
  // A sample from the PC's sound was saved as a WAV file.
  void SampleCaptured(const QString& path);
  // The words heard in the sample at sample_path.
  void Transcribed(const QString& sample_path, const QString& words);
  void Problem(const QString& why);

 private:
  // Why the worker can't take a job: not installed, or busy.
  QString WhyNoWorker() const;
  QString WhyNoSpeak(const QString& text) const;
  Result<void> Start(const VoiceJob& job);
  void SetState(State state);
  void TakeDone(double seconds, const QString& text);
  void FinishCapture();
  QString TempPath(const QString& stem) const;
  QString VoicesFolder() const;

  DocumentManager* document_;
  PlaybackManager* playback_;
  ReelManager* reel_;
  VoiceLibrary* voices_;
  QString data_folder_;
  QTemporaryDir temp_;
  VoiceWorker worker_;
  DesktopRecorder recorder_;
  State state_ = State::kIdle;
  // The line being spoken, and the one the last take came from.
  VoiceJob pending_job_;
  VoiceJob take_job_;
  QString last_take_;
  double last_seconds_ = 0.0;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_VOICE_GEN_MANAGER_H_
