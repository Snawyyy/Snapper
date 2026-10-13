#ifndef SNAPPER_UI_TIMELINE_DRAFT_H_
#define SNAPPER_UI_TIMELINE_DRAFT_H_

#include <QObject>
#include <QPointF>

#include <memory>
#include <optional>
#include <set>

#include "base/frame.h"
#include "edit/edit_scope.h"
#include "ui/managers.h"

class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;
class QPainter;

namespace snapper {

class TimelineView;

// The Pose timeline's Draft row: a dashed row under the reference rows
// with the shot's draft cels as keys, a hold line from each to the
// next, and an eye in its name. It watches the timeline's own mouse and
// keys (an event filter) and paints when the timeline asks, so the
// timeline needs only a hook for each. Click picks a cel (the shared
// pick rule), a drag slides the picked cels as one undo step (Escape
// puts them back), Delete removes them, a double click draws a cel
// there, right click offers the same. It decides nothing: every edit
// is a DraftManager call.
class TimelineDraft final : public QObject {
  Q_OBJECT

 public:
  TimelineDraft(const Managers& managers, TimelineView* view);
  ~TimelineDraft() override;

  // The row's height: one row while there is a shot and drafts are on,
  // else 0, so the rows under it start that much lower.
  double Height() const;
  void Paint(QPainter* painter) const;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  // Where the row starts: under the wave and the reference rows.
  double Top() const;
  bool IsInRow(QPointF point) const;
  // Pixels per frame on the timeline now.
  double FrameWidth() const;
  // The cel starting nearest to x within reach, or nothing.
  std::optional<Frame> CelNear(double x) const;
  ShotId Shot() const;
  bool Press(const QMouseEvent& event);
  bool MoveTo(const QMouseEvent& event);
  bool Release(const QMouseEvent& event);
  bool DoubleClick(const QMouseEvent& event);
  bool Menu(const QContextMenuEvent& event);
  bool Key(const QKeyEvent& event);
  // Puts the playhead on frame of the picked shot.
  void Seek(Frame frame);
  void Report(const QString& problem) const;

  Managers managers_;
  TimelineView* view_;
  // A press on a cel: where, on which frame, the cels it may drag, and
  // whether a plain click on a picked cel narrows the pick on release.
  bool is_pressed_ = false;
  QPointF press_;
  Frame press_frame_;
  std::set<Frame> origin_;
  bool is_narrowing_ = false;
  std::unique_ptr<EditScope> drag_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TIMELINE_DRAFT_H_
