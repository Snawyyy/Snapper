#ifndef SNAPPER_UI_COMP_TIMELINE_H_
#define SNAPPER_UI_COMP_TIMELINE_H_

#include <QRectF>
#include <QWidget>

#include <memory>
#include <vector>

#include "base/frame.h"
#include "edit/edit_scope.h"
#include "model/project.h"
#include "ui/managers.h"

class QPainter;

namespace snapper {

// The Comp tab's timeline: the Video tab's reel with its playhead, a
// row of the shot clips (the one under the playhead is the picked shot),
// then a row per effect of that shot, in the order they run, each a clip
// over the reel showing when it runs and the frames its knobs are keyed
// on, and one row for the shot's Light. Dragging a clip moves it (its
// keys go with it), dragging an end trims it, a double click fits it to
// its shot again; one undo step each, Escape cancels, Ctrl drags without
// snapping. A clip may run past its shot's cut: an effect on the screen
// then goes on over whatever the reel shows; one on a doll stops at the
// cut (that part is hatched). Elsewhere a click or drag moves the
// playhead, which picks the shot there.
class CompTimeline final : public QWidget {
  Q_OBJECT

 public:
  explicit CompTimeline(const Managers& managers);

  QSize sizeHint() const override { return QSize(600, 120); }

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  // One effect's clip as drawn: its box, and its frames of the shot.
  struct Bar final {
    CompEffectId id{};
    QRectF box;
    int first = 0;
    int end = 0;
    bool is_on_doll = false;
  };
  enum class Grab { kNone, kMove, kStart, kEnd };

  // x of a reel frame, and the reel frame at x (unclamped when free).
  double XOf(int frame) const;
  int FrameAt(double x) const;
  int FreeFrameAt(double x) const;
  int Length() const;
  void SeekTo(double x);
  void PaintShots(QPainter* painter, double y) const;
  void PaintBars(QPainter* painter) const;
  void PaintLight(QPainter* painter) const;
  // The picked shot's clip the effects are drawn over; nullptr for none.
  const Clip* ShotClip() const;
  // Every effect's clip, in the order they run (comp_timeline_bars.cpp).
  std::vector<Bar> Bars() const;
  // A drag of a clip (ui/comp_timeline_drag.cpp).
  bool StartDrag(QPointF at);
  void DragTo(QPointF at, bool is_free);
  // frame snapped to the shot clip's ends and the playhead within a few
  // pixels.
  int Snapped(int frame) const;

  Managers managers_;
  Grab grab_ = Grab::kNone;
  CompEffectId dragged_{};
  int press_frame_ = 0;
  int from_first_ = 0;
  int from_end_ = 0;
  std::unique_ptr<EditScope> drag_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_TIMELINE_H_
