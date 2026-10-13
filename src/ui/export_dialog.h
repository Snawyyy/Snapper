#ifndef SNAPPER_UI_EXPORT_DIALOG_H_
#define SNAPPER_UI_EXPORT_DIALOG_H_


#include <QDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "edit/export_manager.h"
#include "ui/choice_row.h"
#include "ui/managers.h"

namespace snapper {

// Export to MP4, GIF, PNG frames, WebM or MOV: pick format, range, size
// and file (a folder for PNG frames), then watch it render. Editing can
// go on while it runs; Cancel stops it and leaves no file. Laid out as
// plated sections (Picture, Frames, File) and sized to the screen.
class ExportDialog final : public QDialog {
  Q_OBJECT

 public:
  ExportDialog(const Managers& managers, QWidget* parent);

  // Shows the dialog in front, fitted to its screen when it was closed.
  void Open();

  // The request the fields describe.
  ExportRequest Request() const;
  void SetPath(const QString& path) { path_.setText(path); }
  // Why Export can't start, for its tooltip; empty when it can.
  QString WhyNotReady() const;

 private:
  void BuildLayout();
  void Wire();
  void Browse();
  void Start();
  // Export sound only: the mix as a WAV, for finishing it elsewhere.
  void ExportSound();
  void Refresh();
  void PickFormat(int index);

  Managers managers_;
  QVBoxLayout layout_;
  // The sections' forms; the dialog owns them through their sections.
  QFormLayout* picture_form_ = nullptr;
  QFormLayout* frames_form_ = nullptr;
  ChoiceRow format_;
  // What the format does with see-through shots (ExportManager).
  QLabel note_;
  ChoiceRow range_;
  QSpinBox from_;
  QSpinBox to_;
  ChoiceRow size_;
  // Where the subtitle track goes: in the picture, a file beside, both.
  ChoiceRow subtitles_;
  QHBoxLayout file_row_;
  QLineEdit path_;
  QPushButton browse_;
  QProgressBar progress_;
  QLabel status_;
  QHBoxLayout buttons_;
  QPushButton sound_only_;
  QPushButton export_;
  QPushButton cancel_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_EXPORT_DIALOG_H_
