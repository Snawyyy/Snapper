#ifndef SNAPPER_UI_VOICE_CLONE_BOX_H_
#define SNAPPER_UI_VOICE_CLONE_BOX_H_

#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/managers.h"

namespace snapper {

class AudioPreview;

// The Voice panel's Clone half, in the panel kit's style: a Voice plate
// (the saved voices, with Rename and Delete) over a Sample plate (open a
// file, record what the PC plays, listen, the words the sample says,
// Save voice). Picking a saved voice loads its sample and words; a new
// sample or recording goes back to "New voice". All of it is asked of
// VoiceLibrary and VoiceGenManager; nothing is decided here.
class VoiceCloneBox final : public QWidget {
  Q_OBJECT

 public:
  VoiceCloneBox(const Managers& managers, AudioPreview* preview);

  // The sample to clone from; empty before one is opened or recorded.
  const QString& sample_path() const { return sample_path_; }
  // What the sample says, trimmed; empty clones from its sound alone.
  QString words() const { return words_.toPlainText().trimmed(); }
  // The language a saved voice is filed under.
  void SetLanguage(const QString& language);
  // Shows the state of the managers; call when they change.
  void Refresh();

 signals:
  // The sample was replaced or a saved voice was picked.
  void Changed();
  // A picked voice last spoke this language.
  void LanguageSuggested(const QString& language);
  void Problem(const QString& why);

 private:
  QWidget* BuildVoiceBody();
  QWidget* BuildSampleBody();
  void Refill(const QString& pick);
  QString PickedVoice() const;
  void LoadPickedVoice();
  void AskForSample();
  // Takes path as a new sample, leaving any saved voice.
  void UseNewSample(const QString& path);
  void Adopt(const QString& path);
  void ToggleCapture();
  void SaveVoice();
  void RenameVoice();
  void DeleteVoice();
  void Report(const QString& problem) {
    const bool has_problem = !problem.isEmpty();
    if (has_problem) {
      emit Problem(problem);
    }
  }

  Managers managers_;
  AudioPreview* preview_;
  QVBoxLayout layout_;
  QComboBox voices_box_;
  QPushButton rename_;
  QPushButton delete_;
  QPushButton open_;
  QPushButton record_;
  QPushButton play_;
  QPushButton save_;
  QLabel length_;
  QLabel* hint_ = nullptr;
  QProgressBar level_;
  QPlainTextEdit words_;
  QString sample_path_;
  QString language_;
  double sample_seconds_ = 0.0;
  double capture_seconds_ = 0.0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_VOICE_CLONE_BOX_H_
