#ifndef SNAPPER_UI_EXPORT_DIALOG_H_
#define SNAPPER_UI_EXPORT_DIALOG_H_

#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>

#include "edit/export_manager.h"
#include "ui/managers.h"

namespace snapper {

// Export to MP4 (with the song) or GIF: pick format, range, size and
// file, then watch it render. Editing can go on while it runs; Cancel
// stops it and leaves no file.
class ExportDialog final : public QDialog {
  Q_OBJECT

 public:
  ExportDialog(const Managers& managers, QWidget* parent);

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
  void Refresh();
  void PickFormat(int index);

  Managers managers_;
  QFormLayout layout_;
  QComboBox format_;
  QComboBox range_;
  QSpinBox from_;
  QSpinBox to_;
  QComboBox size_;
  QHBoxLayout file_row_;
  QLineEdit path_;
  QPushButton browse_;
  QProgressBar progress_;
  QLabel status_;
  QHBoxLayout buttons_;
  QPushButton export_;
  QPushButton cancel_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_EXPORT_DIALOG_H_
