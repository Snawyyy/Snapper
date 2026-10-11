#ifndef SNAPPER_UI_NEW_PROJECT_DIALOG_H_
#define SNAPPER_UI_NEW_PROJECT_DIALOG_H_

#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QString>
#include <QToolButton>
#include <QWidget>

#include <array>

#include "model/project.h"

namespace snapper {

// Asks for a new project's name and frame size, picked on four tiles
// (YouTube, Shorts, square, custom), each drawn as a frame of its shape.
// OK stays greyed out, saying why, until both are usable.
class NewProjectDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit NewProjectDialog(QWidget* parent);

  QString name() const { return name_.text().trimmed(); }
  CanvasSize canvas() const;
  // Why OK can't be pressed; empty when it can.
  QString WhyNotReady() const;

  // For tests and presets: fills the size fields.
  void SetCanvas(CanvasSize canvas);

 private:
  void PickPreset(int index);
  // Presses the tile for index without picking it again.
  void ShowPreset(int index);
  void Refresh();

  QFormLayout layout_;
  QLineEdit name_;
  QWidget tiles_;
  QHBoxLayout tiles_layout_;
  QButtonGroup group_;
  std::array<QToolButton*, 4> tile_{};
  QSpinBox width_;
  QSpinBox height_;
  QDialogButtonBox buttons_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_NEW_PROJECT_DIALOG_H_
