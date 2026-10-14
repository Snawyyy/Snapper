#ifndef SNAPPER_UI_TIMELINE_STORYBOARD_H_
#define SNAPPER_UI_TIMELINE_STORYBOARD_H_

#include <QObject>
#include <QPointF>

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/frame.h"
#include "edit/edit_scope.h"
#include "model/layer.h"
#include "ui/managers.h"

class QContextMenuEvent;
class QKeyEvent;
class QMenu;
class QMouseEvent;
class QPainter;

namespace snapper {

class TimelineView;

// The Pose timeline's storyboard rows: a dashed row under the reference
// rows for each storyboard layer of the shot, topmost layer first, with
// the layer's name and eye, its frames (cels) as keys and a hold line
// from each to the next. It watches the timeline's own mouse and keys
// (an event filter) and paints when the timeline asks, so the timeline
// needs only a hook for each. Click picks a frame (the shared pick
// rule), a drag slides the picked frames as one undo step (Escape puts
// them back), Delete removes them, a double click draws a frame there
// (over the one starting there), right click offers the same. It
// decides nothing: every edit is a StoryboardManager or StageManager
// call.
class TimelineStoryboard final : public QObject {
  Q_OBJECT

 public:
  TimelineStoryboard(const Managers& managers, TimelineView* view);
  ~TimelineStoryboard() override;

  // The rows' height: one row per storyboard layer of the picked shot,
  // so the rows under them start that much lower.
  double Height() const;
  void Paint(QPainter* painter) const;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  // The shot's storyboard layers, topmost first.
  std::vector<LayerId> Rows() const;
  // Where the rows start: under the wave and the reference rows.
  double Top() const;
  // The row under point, or -1 (and the layer it shows).
  int RowAt(QPointF point) const;
  LayerId LayerAt(QPointF point) const;
  // Pixels per frame on the timeline now.
  double FrameWidth() const;
  // The frame of layer starting nearest to x within reach, or nothing.
  std::optional<Frame> CelNear(LayerId layer, double x) const;
  ShotId Shot() const;
  void PaintRow(QPainter* painter, const Layer& layer, double top,
                Frame shot_length) const;
  bool Press(const QMouseEvent& event);
  bool MoveTo(const QMouseEvent& event);
  bool Release(const QMouseEvent& event);
  bool DoubleClick(const QMouseEvent& event);
  bool Menu(const QContextMenuEvent& event);
  bool Key(const QKeyEvent& event);
  // The row menu's entries for layer: New frame here, Draw over, Delete,
  // the eye and In export, each greyed with its reason when it can't
  // act.
  void AddMenuActions(QMenu* menu, LayerId layer);
  // Starts the one undo step of a drag of the picked frames of layer.
  void StartDrag(LayerId layer);
  // Escape in a drag: lands nothing and puts the pick back.
  void CancelDrag();
  // Puts the playhead on frame of the picked shot.
  void Seek(Frame frame);
  // Makes layer the one New frame and the box act on, with no frames
  // picked.
  void Aim(LayerId layer);
  void Report(const QString& problem) const;

  Managers managers_;
  TimelineView* view_;
  // A press on a frame: where, on which frame and layer, the frames it
  // may drag, and whether a plain click on a picked frame narrows the
  // pick on release.
  bool is_pressed_ = false;
  QPointF press_;
  Frame press_frame_;
  LayerId press_layer_;
  std::set<Frame> origin_;
  bool is_narrowing_ = false;
  std::unique_ptr<EditScope> drag_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TIMELINE_STORYBOARD_H_
