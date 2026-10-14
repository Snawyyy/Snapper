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
#include "ui/hero_button.h"
#include "ui/managers.h"
#include "ui/voice_clone_box.h"
#include "ui/voice_engine_box.h"
#include "ui/wave_screen.h"

namespace snapper {

// The Video tab's Voice panel, always shown beside the clip panels: two
// mode buttons, Clone and Design (remembered), then numbered steps:
// the voice (a saved voice or a sample to clone, or a description to
// design one from), the line to say with its language and the big red
// Generate (Ctrl+Enter; a click while it works stops it), and Listen:
// the line on a phosphor screen to play, then Add to timeline at the
// playhead. Built with the panel kit; every decision is
// VoiceGenManager's and VoiceLibrary's, the panel only asks and shows
// (greyed controls carry the manager's reason).
class VoicePanel final : public QScrollArea {
  Q_OBJECT

 public:
  explicit VoicePanel(const Managers& managers);

  // Shows the managers' state; call when it changes.
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  QWidget* BuildModeRow(QWidget* parent);
  QWidget* BuildDesignPage(QWidget* parent);
  QWidget* BuildLineBody(QWidget* parent);
  QWidget* BuildResultBody(QWidget* parent);
  void Wire();
  bool IsDesign() const { return is_design_; }
  QString Language() const;
  void ShowLanguage(const QString& language);
  void SetMode(bool is_design);
  void ShowMode();
  void ShowGenerate();
  // Generate, or stops the line while one is being made.
  void GenerateOrStop();
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
  bool is_design_ = false;
  QPushButton clone_mode_;
  QPushButton design_mode_;
  QLabel mode_caption_;
  VoiceCloneBox clone_;
  VoiceEngineBox engine_box_;
  QWidget* design_page_ = nullptr;
  QWidget* line_section_ = nullptr;
  QWidget* result_section_ = nullptr;
  QPlainTextEdit describe_;
  QPushButton save_designed_;
  QPlainTextEdit line_;
  QComboBox language_;
  HeroButton generate_;
  WaveScreen take_screen_;
  QPushButton play_take_;
  QPushButton add_;
  QShortcut generate_key_;
  QShortcut generate_pad_key_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_VOICE_PANEL_H_
