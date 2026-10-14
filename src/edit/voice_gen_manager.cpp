#include "edit/voice_gen_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cassert>

#include "base/text.h"
#include "edit/document_manager.h"
#include "edit/playback_manager.h"
#include "edit/reel_manager.h"
#include "edit/sidecar_folder.h"
#include "edit/voice_library.h"
#include "media/audio_clip.h"
#include "media/wav_file.h"

namespace snapper {
namespace {

constexpr const char* kScriptName = "voice_gen.py";

}  // namespace

QString VoicePython(const QString& data_folder, VoiceEngine engine) {
  assert(!data_folder.isEmpty());
  assert(kScriptName != nullptr);
  const bool is_chatterbox = engine == VoiceEngine::kChatterbox;
  const bool is_index = engine == VoiceEngine::kIndex;
  return QDir(data_folder).filePath(
      is_chatterbox ? QStringLiteral("voice_gen/chatterbox_venv/bin/python")
      : is_index    ? QStringLiteral("voice_gen/index-tts/.venv/bin/python")
                    : QStringLiteral("voice_gen/venv/bin/python"));
}

QString VoiceEngineName(VoiceEngine engine) {
  assert(static_cast<int>(engine) >= 0);
  assert(static_cast<int>(engine) < kVoiceEngineCount);
  const bool is_chatterbox = engine == VoiceEngine::kChatterbox;
  const bool is_index = engine == VoiceEngine::kIndex;
  return is_chatterbox ? Tr("Chatterbox")
         : is_index    ? Tr("IndexTTS")
                       : Tr("Qwen3");
}

VoiceGenManager::VoiceGenManager(DocumentManager* document,
                                 PlaybackManager* playback,
                                 ReelManager* reel, VoiceLibrary* voices,
                                 const QString& data_folder)
    : document_(document),
      playback_(playback),
      reel_(reel),
      voices_(voices),
      data_folder_(data_folder),
      worker_(VoicePython(data_folder, VoiceEngine::kQwen),
              QDir(QCoreApplication::applicationDirPath())
                  .filePath(QString::fromLatin1(kScriptName))) {
  assert(document_ != nullptr && playback_ != nullptr);
  assert(reel_ != nullptr && voices_ != nullptr);
  assert(temp_.isValid());
  // A transcription stays Listening while its model loads and runs.
  connect(&worker_, &VoiceWorker::Loading, this, [this] {
    const bool is_speech = pending_job_.kind != VoiceKind::kTranscribe;
    SetState(is_speech ? State::kLoading : State::kListening);
  });
  connect(&worker_, &VoiceWorker::Speaking, this, [this] {
    const bool is_speech = pending_job_.kind != VoiceKind::kTranscribe;
    SetState(is_speech ? State::kSpeaking : State::kListening);
  });
  connect(&worker_, &VoiceWorker::Done, this, &VoiceGenManager::TakeDone);
  connect(&worker_, &VoiceWorker::Failed, this, [this](const QString& why) {
    SetState(State::kIdle);
    emit Problem(why);
  });
  connect(&recorder_, &DesktopRecorder::Level, this,
          &VoiceGenManager::CaptureLevel);
  connect(&recorder_, &DesktopRecorder::Seconds, this,
          &VoiceGenManager::CaptureSeconds);
  connect(&recorder_, &DesktopRecorder::LimitReached, this,
          &VoiceGenManager::FinishCapture);
}

VoiceGenManager::~VoiceGenManager() {
  assert(temp_.isValid());
  assert(voices_ != nullptr);
  worker_.Cancel();
  recorder_.Stop();
}

Result<void> VoiceGenManager::Transcribe(const QString& sample_path) {
  assert(voices_ != nullptr);
  assert(temp_.isValid());
  const QString why_not = WhyNoTranscribe(sample_path);
  const bool is_blocked = !why_not.isEmpty();
  if (is_blocked) {
    return std::unexpected(Error{why_not});
  }
  VoiceJob job;
  job.kind = VoiceKind::kTranscribe;
  job.sample = QFileInfo(sample_path).absoluteFilePath();
  pending_job_ = job;
  const auto sent = worker_.Submit(pending_job_);
  const bool is_sent = sent.has_value();
  if (is_sent) {
    SetState(State::kListening);
  }
  return sent;
}

Result<void> VoiceGenManager::SpeakClone(const QString& sample_path,
                                         const QString& words,
                                         const QString& text,
                                         const QString& language,
                                         VoiceEngine engine, double emotion) {
  assert(voices_ != nullptr);
  assert(emotion >= kMinEmotion && emotion <= kMaxEmotion);
  const QString why_not =
      WhyNoSpeakClone(sample_path, text, engine, language);
  const bool is_blocked = !why_not.isEmpty();
  if (is_blocked) {
    return std::unexpected(Error{why_not});
  }
  VoiceJob job;
  job.text = text.trimmed();
  job.language = language;
  job.sample = QFileInfo(sample_path).absoluteFilePath();
  job.words = words.trimmed();
  job.engine = engine;
  job.emotion = emotion;
  return Start(job);
}

Result<void> VoiceGenManager::SpeakDesign(const QString& describe,
                                          const QString& text,
                                          const QString& language) {
  assert(voices_ != nullptr);
  assert(temp_.isValid());
  const QString why_not = WhyNoSpeakDesign(describe, text);
  const bool is_blocked = !why_not.isEmpty();
  if (is_blocked) {
    return std::unexpected(Error{why_not});
  }
  VoiceJob job;
  job.kind = VoiceKind::kDesign;
  job.text = text.trimmed();
  job.language = language;
  job.describe = describe.trimmed();
  return Start(job);
}

Result<void> VoiceGenManager::Start(const VoiceJob& job) {
  assert(!job.text.isEmpty());
  assert(temp_.isValid());
  pending_job_ = job;
  pending_job_.out = TempPath(QStringLiteral("line"));
  // Another engine's worker goes first, so one model set fills the card.
  worker_.SetPython(VoicePython(data_folder_, job.engine));
  const bool is_warm = worker_.IsRunning();
  const auto sent = worker_.Submit(pending_job_);
  if (!sent) {
    return sent;
  }
  // A worker just started has no model yet; one that runs may have it.
  SetState(is_warm ? State::kSpeaking : State::kLoading);
  return {};
}

void VoiceGenManager::TakeDone(double seconds, const QString& text) {
  assert(seconds >= 0.0);
  assert(text.size() < 1000000);
  const bool is_transcript = pending_job_.kind == VoiceKind::kTranscribe;
  if (is_transcript) {
    SetState(State::kIdle);
    emit Transcribed(pending_job_.sample, text.trimmed());
    return;
  }
  const bool has_file = QFileInfo(pending_job_.out).isFile();
  SetState(State::kIdle);
  if (!has_file) {
    emit Problem(Tr("The voice worker finished without saving the line."));
    return;
  }
  take_job_ = pending_job_;
  last_take_ = pending_job_.out;
  last_seconds_ = seconds;
  emit TakeReady();
}

void VoiceGenManager::Cancel() {
  assert(temp_.isValid());
  assert(voices_ != nullptr);
  worker_.Cancel();
  SetState(State::kIdle);
}

void VoiceGenManager::SetState(State state) {
  assert(temp_.isValid());
  assert(state == State::kIdle || state == State::kLoading ||
         state == State::kSpeaking || state == State::kListening);
  const bool is_new = state != state_;
  state_ = state;
  if (is_new) {
    emit StateChanged();
  }
}

QString VoiceGenManager::TempPath(const QString& stem) const {
  assert(temp_.isValid());
  assert(!stem.isEmpty());
  return FreshNumberedPath(QDir(temp_.path()), stem, QStringLiteral("wav"));
}

QString VoiceGenManager::last_voice_name() const {
  assert(voices_ != nullptr);
  assert(temp_.isValid());
  const bool is_designed = take_job_.kind == VoiceKind::kDesign;
  if (is_designed) {
    return Tr("Designed voice");
  }
  for (const Voice& voice : voices_->Voices()) {
    const bool is_match = voice.sample == take_job_.sample;
    if (is_match) {
      return voice.name;
    }
  }
  return Tr("Sample");
}

QString VoiceGenManager::VoicesFolder() const {
  assert(document_ != nullptr);
  assert(!data_folder_.isEmpty());
  const bool is_saved = document_->HasPath();
  return SidecarFolder(is_saved ? document_->path() : QString(), data_folder_,
                       Tr("%1 voices"), QStringLiteral("voices_used"));
}

Result<ClipId> VoiceGenManager::AddToReel() {
  assert(reel_ != nullptr);
  assert(playback_ != nullptr);
  const QString why_not = WhyNoAdd();
  const bool is_blocked = !why_not.isEmpty();
  if (is_blocked) {
    return std::unexpected(Error{why_not});
  }
  const QDir folder(VoicesFolder());
  const QString path = FreshNumberedPath(
      folder, SafeFileName(last_voice_name()), QStringLiteral("wav"));
  const bool is_copied =
      QDir().mkpath(folder.path()) && QFile::copy(last_take_, path);
  if (!is_copied) {
    return std::unexpected(Error{Tr("Could not save the line to %1.")
                                     .arg(QDir::toNativeSeparators(path))});
  }
  const auto made = reel_->AddSounds(
      {path}, -1, playback_->FrameOn(Timeline::kReel));
  if (!made) {
    QFile::remove(path);
    return std::unexpected(made.error());
  }
  assert(!made->empty());
  return made->front();
}

Result<void> VoiceGenManager::SaveDesigned(const QString& name,
                                           bool replace) {
  assert(voices_ != nullptr);
  assert(temp_.isValid());
  const QString why_not = WhyNoSaveDesigned();
  const bool is_blocked = !why_not.isEmpty();
  if (is_blocked) {
    return std::unexpected(Error{why_not});
  }
  return voices_->Save(name, last_take_, take_job_.text, take_job_.language,
                       take_job_.describe, replace);
}

Result<void> VoiceGenManager::ToggleCapture() {
  assert(temp_.isValid());
  assert(kMaxSampleSeconds > 0);
  const bool is_capturing = IsCapturing();
  if (is_capturing) {
    FinishCapture();
    return {};
  }
  const auto started = recorder_.Start();
  if (!started) {
    return started;
  }
  emit StateChanged();
  return {};
}

void VoiceGenManager::FinishCapture() {
  assert(temp_.isValid());
  assert(kMaxSampleSeconds > 0);
  const Take take = recorder_.Stop();
  emit StateChanged();
  const bool is_empty = take.samples.empty();
  if (is_empty) {
    emit Problem(Tr("Nothing was heard from the PC's sound."));
    return;
  }
  const QString path = TempPath(QStringLiteral("sample"));
  QFile file(path);
  const bool is_written =
      file.open(QIODevice::WriteOnly) &&
      file.write(WaveFile(take.rate, take.channels, take.samples)) > 0;
  file.close();
  if (!is_written) {
    emit Problem(Tr("Could not save the sample to %1.")
                     .arg(QDir::toNativeSeparators(path)));
    return;
  }
  emit SampleCaptured(path);
}

Result<double> VoiceGenManager::SampleSeconds(const QString& path) const {
  assert(temp_.isValid());
  assert(!path.isEmpty());
  return ProbeSoundSeconds(path);
}

}  // namespace snapper
