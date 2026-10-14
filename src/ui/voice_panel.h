#ifndef SNAPPER_UI_VOICE_PANEL_H_
#define SNAPPER_UI_VOICE_PANEL_H_

#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QString>
#include <QWidget>

#include "ui/audio_preview.h"
#include "ui/choice_row.h"
#include "ui/managers.h"
#include "ui/voice_clone_box.h"

namespace snapper {

// The Video tab's Voice panel, always shown beside the clip panels: a
// Clone | Design switch (remembered), the voice (a saved voice or a
// sample to clone, or a description to design one from), the line to
// say with its language, Generate (Ctrl+Enter, with Cancel while it
// works), and the result: listen, then Add to timeline at the playhead.
// Built with the panel kit; every decision is VoiceGenManager's and
// VoiceLibrary's, the panel only asks and shows (greyed controls carry
// the manager's reason).
class VoicePanel final : public QScrollArea {
  Q_OBJECT

 public:
  explicit VoicePanel(const Managers& managers);

  // Shows the managers' state; call when it changes.
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  QWidget* BuildDesignPage(QWidget* parent);
  QWidget* BuildLineBody(QWidget* parent);
  QWidget* BuildResultBody(QWidget* parent);
  void Wire();
  bool IsDesign() const;
  QString Language() const;
  void ShowLanguage(const QString& language);
  void ShowMode();
  void Generate();
  void AddToTimeline();
  void SaveDesigned();
  void Report(const QString& problem) {
    const bool has_problem = !problem.isEmpty();
    if (has_problem) {
      emit Problem(problem);
    }
  }

  Managers managers_;
  AudioPreview preview_;
  ChoiceRow mode_;
  VoiceCloneBox clone_;
  QWidget* design_page_ = nullptr;
  QPlainTextEdit describe_;
  QPushButton save_designed_;
  QPlainTextEdit line_;
  QComboBox language_;
  QPushButton generate_;
  QPushButton cancel_;
  QPushButton play_take_;
  QPushButton add_;
  QLabel take_info_;
  QShortcut generate_key_;
  QShortcut generate_pad_key_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_VOICE_PANEL_H_
