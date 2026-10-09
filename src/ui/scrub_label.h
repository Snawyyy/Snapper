#ifndef SNAPPER_UI_SCRUB_LABEL_H_
#define SNAPPER_UI_SCRUB_LABEL_H_

#include <QAbstractSpinBox>
#include <QLabel>
#include <QString>

#include <optional>

#include "base/scrub_drag.h"

class QEvent;
class QKeyEvent;
class QMouseEvent;

namespace snapper {

class LiveEdit;

// The name beside a number field, which also works it: drag the name
// sideways to change the number (see ScrubDrag for the feel). The
// field's own live edit takes the changes, so one drag is one undo
// step; Escape puts the number back and cancels it. A plain click
// puts the cursor in the field instead. It greys out with the field
// and shares its tooltip.
//
// live may be null for a field that edits no project.
class ScrubLabel final : public QLabel {
  Q_OBJECT

 public:
  ScrubLabel(const QString& text, QAbstractSpinBox* box, LiveEdit* live);

  bool is_dragging() const { return drag_.has_value(); }

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  double ValueOfBox() const;
  void PutInBox(double value);
  ScrubRange RangeOfBox(double unit) const;
  void SyncWithBox();
  void End(bool is_kept);

  QAbstractSpinBox* box_;
  LiveEdit* live_;
  std::optional<ScrubDrag> drag_;
  int press_x_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SCRUB_LABEL_H_
