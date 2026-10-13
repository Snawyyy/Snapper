#ifndef SNAPPER_UI_COMP_TIMELINE_H_
#define SNAPPER_UI_COMP_TIMELINE_H_

#include <QWidget>

#include "base/frame.h"
#include "ui/managers.h"

namespace snapper {

// The Comp tab's timeline: the shot's frames with the playhead, a row
// per effect (top of the stack first) showing the frames its knobs are
// keyed on, and one for the shot's Light. Click or drag to move the
// playhead; clicking a row also picks its effect.
class CompTimeline final : public QWidget {
  Q_OBJECT

 public:
  explicit CompTimeline(const Managers& managers);

  QSize sizeHint() const override { return QSize(600, 120); }

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;

 private:
  // x of a shot frame, and the frame at x.
  double XOf(int frame) const;
  int FrameAt(double x) const;
  int Length() const;
  void SeekTo(double x);

  Managers managers_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_TIMELINE_H_
