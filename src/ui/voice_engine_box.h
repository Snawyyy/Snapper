#ifndef SNAPPER_UI_VOICE_ENGINE_BOX_H_
#define SNAPPER_UI_VOICE_ENGINE_BOX_H_

#include <QDoubleSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

#include "media/voice_worker.h"
#include "ui/choice_row.h"
#include "ui/managers.h"

namespace snapper {

// The Voice panel's clone settings: which model clones (Qwen3, Chatterbox
// or IndexTTS, whose {feeling} marks a hint explains) and, for the last
// two, how strongly to feel, from flat to wild on a
// fader (double click puts it back to plain). Both are remembered. The
// fader is greyed with VoiceGenManager's reason when the model can't
// take an emotion, and a model not installed says so.
class VoiceEngineBox final : public QWidget {
  Q_OBJECT

 public:
  explicit VoiceEngineBox(const Managers& managers);

  VoiceEngine engine() const;
  double emotion() const { return emotion_.value(); }
  // Shows the manager's reasons; call when they may have changed.
  void Refresh();

 signals:
  void Changed();

 private:
  void Remember() const;

  Managers managers_;
  QVBoxLayout column_;
  ChoiceRow engine_;
  QDoubleSpinBox emotion_;
  QWidget* emotion_row_ = nullptr;
  // How to mark feelings; shown for IndexTTS only.
  QLabel* feelings_hint_ = nullptr;
};

}  // namespace snapper

#endif  // SNAPPER_UI_VOICE_ENGINE_BOX_H_
