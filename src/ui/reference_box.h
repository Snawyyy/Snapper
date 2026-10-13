#ifndef SNAPPER_UI_REFERENCE_BOX_H_
#define SNAPPER_UI_REFERENCE_BOX_H_

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QString>

#include "model/reference.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// The picked shot's references: which one (the picked row, or the first),
// its opacity, behind or in front, its eye, flip, tint, the fit tiles,
// when it starts, where in the file, its speed, its markers, finding a
// moved file and removing it. The inspector shows it only while one of
// the shot's references is picked. Every change goes through the
// ReferenceManager.
class ReferenceBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit ReferenceBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  void BuildRows();
  // The Look section (opacity, front, eye, flip, tint, fit) and the
  // Plays section (start, slide, speed, markers).
  void BuildLook();
  void BuildTime();
  void Wire();
  // The reference the box shows: the picked one, else the first; invalid
  // when the shot has none.
  ReferenceId Current() const;
  ShotId Shot() const;
  void Report(const QString& why) { emit Problem(why); }

  Managers managers_;
  QFormLayout layout_;
  LiveEdit live_;
  QComboBox which_;
  QLabel missing_;
  QPushButton find_;
  QDoubleSpinBox opacity_;
  QHBoxLayout switch_row_;
  QCheckBox front_;
  QCheckBox shown_;
  QHBoxLayout look_row_;
  QCheckBox flip_;
  QComboBox tint_;
  QHBoxLayout fit_row_;
  QPushButton fit_frame_;
  QPushButton fit_height_;
  QSpinBox start_;
  QDoubleSpinBox skip_;
  QDoubleSpinBox speed_;
  QHBoxLayout marks_row_;
  QLabel marks_;
  QPushButton clear_marks_;
  QPushButton remove_;
  QLabel hint_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_REFERENCE_BOX_H_
