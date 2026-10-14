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
constexpr int kChatterboxIndex = 1;

}  // namespace

VoiceEngineBox::VoiceEngineBox(const Managers& managers)
    : managers_(managers), column_(this) {
  assert(managers_.voice_gen != nullptr);
  assert(managers_.document != nullptr);
  column_.setContentsMargins(0, 0, 0, 0);
  column_.setSpacing(4);
  engine_.addItem(tr("Qwen3"));
  engine_.addItem(tr("Chatterbox"));
  // The two models share the row's width, like tiles.
  for (QToolButton* choice : engine_.findChildren<QToolButton*>()) {
    choice->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  }
  engine_.setToolTip(tr("Qwen3 follows the sample closely and copies its "
                        "mood. Chatterbox needs no words and lets you set "
                        "the emotion."));
  column_.addWidget(MakeNamedRow(tr("Model"), &engine_, this));
  emotion_.setRange(kMinEmotion, kMaxEmotion);
  emotion_.setSingleStep(0.05);
  emotion_.setDecimals(2);
  emotion_row_ = MakeFaderRow(tr("Emotion"), &emotion_, nullptr,
                              kMaxEmotion, kPlainEmotion, this);
  column_.addWidget(emotion_row_);
  const QSettings settings(managers_.document->SettingsPath(),
                           QSettings::IniFormat);
  engine_.setCurrentIndex(
      settings.value(QString::fromLatin1(kEngineKey), 0).toInt() ==
              kChatterboxIndex
          ? kChatterboxIndex
          : 0);
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
  assert(engine_.count() == 2);
  assert(kChatterboxIndex == 1);
  return engine_.currentIndex() == kChatterboxIndex ? VoiceEngine::kChatterbox
                                                    : VoiceEngine::kQwen;
}

void VoiceEngineBox::Remember() const {
  assert(managers_.document != nullptr);
  assert(engine_.count() == 2);
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
  Explain(emotion_row_, no_emotion.isEmpty() ? no_engine : no_emotion,
          tr("How much feeling: low is flat, %1 is plain speech, high is "
             "wild. Strong feeling also slows the pace a little.")
              .arg(kPlainEmotion));
}

}  // namespace snapper
