#ifndef SNAPPER_UI_COMP_TIMELINE_H_
#define SNAPPER_UI_COMP_TIMELINE_H_

#include <QWidget>

#include "base/frame.h"
#include "model/project.h"
#include "ui/managers.h"

class QPainter;

namespace snapper {

// The Comp tab's timeline: the Video tab's reel with its playhead, a
// row of the shot clips (the one under the playhead is the picked shot),
// then a row per effect of that shot (top of the stack first) showing
// the frames its knobs are keyed on over its clip, and one for the
// shot's Light. Click or drag to move the playhead, which picks the
// shot there; clicking a row also picks its effect.
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
  // x of a reel frame, and the reel frame at x.
  double XOf(int frame) const;
  int FrameAt(double x) const;
  int Length() const;
  void SeekTo(double x);
  void PaintShots(QPainter* painter, double y) const;
  void PaintEffects(QPainter* painter, const Shot& shot, const Clip& clip,
                    double top) const;

  Managers managers_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_TIMELINE_H_
