#include "edit/voice_gen_manager.h"

#include <QFileInfo>

#include <cassert>

#include "base/text.h"

// VoiceGenManager's checks: why a job, a line or a sample can't go now.
// Each answers in words for a tooltip, empty when it can.

namespace snapper {

QString VoiceGenManager::WhyNoWorker() const {
  assert(voices_ != nullptr);
  assert(temp_.isValid());
  const bool is_installed = worker_.IsInstalled();
  if (!is_installed) {
    return Tr("Voice gen is not installed: run tools/voice_gen/setup.sh.");
  }
  const bool is_listening = state_ == State::kListening;
  if (is_listening) {
    return Tr("Still writing down the sample's words.");
  }
  return IsBusy() ? Tr("Already speaking.") : QString();
}

QString VoiceGenManager::WhyNoSpeak(const QString& text) const {
  assert(voices_ != nullptr);
  assert(text.size() < 1000000);
  const QString why_not = WhyNoWorker();
  const bool is_blocked = !why_not.isEmpty();
  if (is_blocked) {
    return why_not;
  }
  const bool is_blank = text.trimmed().isEmpty();
  return is_blank ? Tr("Type a line to say.") : QString();
}

QString VoiceGenManager::WhyNoTranscribe(const QString& sample_path) const {
  assert(voices_ != nullptr);
  assert(sample_path.size() < 100000);
  const bool has_sample = QFileInfo(sample_path).isFile();
  if (!has_sample) {
    return Tr("Open or record a sample first.");
  }
  return WhyNoWorker();
}

QString VoiceGenManager::WhyNoEngine(VoiceEngine engine) const {
  assert(!data_folder_.isEmpty());
  assert(temp_.isValid());
  const bool is_installed =
      QFileInfo::exists(VoicePython(data_folder_, engine));
  if (is_installed) {
    return QString();
  }
  return Tr("%1 is not installed: run tools/voice_gen/setup.sh.")
      .arg(VoiceEngineName(engine));
}

QString VoiceGenManager::WhyNoEmotion(VoiceEngine engine) const {
  assert(temp_.isValid());
  assert(voices_ != nullptr);
  const bool is_qwen = engine == VoiceEngine::kQwen;
  return is_qwen ? Tr("Qwen copies the mood of the sample. Pick Chatterbox "
                      "or IndexTTS to set the emotion here.")
                 : QString();
}

QString VoiceGenManager::WhyNoSpeakClone(const QString& sample_path,
                                         const QString& text,
                                         VoiceEngine engine,
                                         const QString& language) const {
  assert(voices_ != nullptr);
  assert(temp_.isValid());
  const bool is_index = engine == VoiceEngine::kIndex;
  const bool is_index_language =
      language == QStringLiteral("Auto") ||
      language == QStringLiteral("English") ||
      language == QStringLiteral("Chinese") ||
      language == QStringLiteral("Japanese") ||
      language == QStringLiteral("Spanish");
  const bool is_unspoken = is_index && !is_index_language;
  if (is_unspoken) {
    return Tr("IndexTTS speaks English, Chinese, Japanese and Spanish.");
  }
  const QString no_engine = WhyNoEngine(engine);
  const bool has_no_engine = !no_engine.isEmpty();
  if (has_no_engine) {
    return no_engine;
  }
  const QString why_not = WhyNoSpeak(text);
  const bool is_blocked = !why_not.isEmpty();
  if (is_blocked) {
    return why_not;
  }
  const bool has_sample = QFileInfo(sample_path).isFile();
  return has_sample ? QString() : Tr("Pick a voice or a sample first.");
}

QString VoiceGenManager::WhyNoSpeakDesign(const QString& describe,
                                          const QString& text) const {
  assert(voices_ != nullptr);
  assert(temp_.isValid());
  // Designs are Qwen's only.
  const QString no_qwen = WhyNoEngine(VoiceEngine::kQwen);
  const QString why_not = no_qwen.isEmpty() ? WhyNoSpeak(text) : no_qwen;
  const bool is_blocked = !why_not.isEmpty();
  if (is_blocked) {
    return why_not;
  }
  const bool has_description = !describe.trimmed().isEmpty();
  return has_description ? QString() : Tr("Describe the voice first.");
}

QString VoiceGenManager::WhyNoAdd() const {
  assert(temp_.isValid());
  assert(reel_ != nullptr);
  const bool has_take = !last_take_.isEmpty() && QFileInfo(last_take_).isFile();
  return has_take ? QString() : Tr("Generate a line first.");
}

QString VoiceGenManager::WhyNoSaveDesigned() const {
  assert(temp_.isValid());
  assert(voices_ != nullptr);
  const bool has_designed =
      WhyNoAdd().isEmpty() && take_job_.kind == VoiceKind::kDesign;
  return has_designed ? QString() : Tr("Generate a line first.");
}

QString VoiceGenManager::WhyNoCapture() const {
  assert(temp_.isValid());
  assert(kMaxSampleSeconds > 0);
  return IsCapturing() ? QString() : DesktopRecorder::WhyNoDesktop();
}

}  // namespace snapper
