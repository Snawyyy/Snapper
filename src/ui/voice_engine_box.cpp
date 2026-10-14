#include "ui/voice_engine_box.h"

#include <QSettings>
#include <QToolButton>

#include <cassert>

#include "edit/document_manager.h"
#include "edit/voice_gen_manager.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/pose_controls.h"

namespace snapper {
namespace {

constexpr const char* kEngineKey = "voice/engine";
constexpr const char* kEmotionKey = "voice/emotion";

}  // namespace

VoiceEngineBox::VoiceEngineBox(const Managers& managers)
    : managers_(managers), column_(this) {
  assert(managers_.voice_gen != nullptr);
  assert(managers_.document != nullptr);
  column_.setContentsMargins(0, 0, 0, 0);
  column_.setSpacing(4);
  for (int engine = 0; engine < kVoiceEngineCount; ++engine) {
    engine_.addItem(VoiceEngineName(static_cast<VoiceEngine>(engine)));
  }
  // The models share the row's width, like tiles.
  for (QToolButton* choice : engine_.findChildren<QToolButton*>()) {
    choice->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  }
  engine_.setToolTip(tr("Qwen3 follows the sample closely and copies its "
                        "mood. Chatterbox needs no words and takes one "
                        "emotion for the line. IndexTTS takes a feeling "
                        "for each sentence: {mad}, {confused}..."));
  column_.addWidget(MakeNamedRow(tr("Model"), &engine_, this));
  feelings_hint_ = EmptyHint(
      tr("Put a feeling after each sentence: Who are you? {confused} "
         "Get out! {mad}"),
      this);
  column_.addWidget(feelings_hint_);
  emotion_.setRange(kMinEmotion, kMaxEmotion);
  emotion_.setSingleStep(0.05);
  emotion_.setDecimals(2);
  emotion_row_ = MakeFaderRow(tr("Emotion"), &emotion_, nullptr,
                              kMaxEmotion, kPlainEmotion, this);
  column_.addWidget(emotion_row_);
  const QSettings settings(managers_.document->SettingsPath(),
                           QSettings::IniFormat);
  const int saved = settings.value(QString::fromLatin1(kEngineKey), 0).toInt();
  const bool is_known = saved >= 0 && saved < kVoiceEngineCount;
  engine_.setCurrentIndex(is_known ? saved : 0);
  emotion_.setValue(
      settings.value(QString::fromLatin1(kEmotionKey), kPlainEmotion)
          .toDouble());
  connect(&engine_, &ChoiceRow::currentIndexChanged, this, [this] {
    Remember();
    Refresh();
    emit Changed();
  });
  connect(&emotion_, &QDoubleSpinBox::valueChanged, this,
          [this] { Remember(); });
  Refresh();
}

VoiceEngine VoiceEngineBox::engine() const {
  assert(engine_.count() == kVoiceEngineCount);
  assert(engine_.currentIndex() < kVoiceEngineCount);
  const int picked = engine_.currentIndex();
  return picked >= 0 ? static_cast<VoiceEngine>(picked) : VoiceEngine::kQwen;
}

void VoiceEngineBox::Remember() const {
  assert(managers_.document != nullptr);
  assert(engine_.count() == kVoiceEngineCount);
  QSettings settings(managers_.document->SettingsPath(),
                     QSettings::IniFormat);
  settings.setValue(QString::fromLatin1(kEngineKey), engine_.currentIndex());
  settings.setValue(QString::fromLatin1(kEmotionKey), emotion_.value());
}

void VoiceEngineBox::Refresh() {
  assert(managers_.voice_gen != nullptr);
  assert(emotion_row_ != nullptr);
  const VoiceGenManager& gen = *managers_.voice_gen;
  const VoiceEngine picked = engine();
  const QString no_engine = gen.WhyNoEngine(picked);
  const QString no_emotion = gen.WhyNoEmotion(picked);
  feelings_hint_->setVisible(picked == VoiceEngine::kIndex);
  Explain(emotion_row_, no_emotion.isEmpty() ? no_engine : no_emotion,
          tr("How much feeling: low is flat, %1 is plain speech, high is "
             "wild. Strong feeling also slows the pace a little.")
              .arg(kPlainEmotion));
}

}  // namespace snapper
