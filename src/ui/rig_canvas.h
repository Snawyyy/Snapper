#ifndef SNAPPER_UI_RIG_CANVAS_H_
#define SNAPPER_UI_RIG_CANVAS_H_

#include <QString>
#include <QStringList>
#include <QTransform>
#include <QWidget>

#include <memory>
#include <set>

#include "edit/edit_scope.h"
#include "edit/selection_manager.h"
#include "render/image_cache.h"
#include "ui/managers.h"

namespace snapper {

// A doll at rest, for jointing it. Click a piece to pick it; drag its
// yellow joint to move the pivot; Shift adds pieces, Ctrl flips them,
// a box on empty space picks what it touches, and joints of all picked
// pieces drag together; double-click a joint, then click the part the
// picked pieces should hang from; drag a square IK tip to set where the chain
// reaches from. Lines join each joint to its parent's. Escape cancels a
// drag or a hang.
class RigCanvas final : public QWidget {
  Q_OBJECT

 public:
  explicit RigCanvas(const Managers& managers);

  void SetDoll(const QString& doll);
  void SetPiece(const QString& piece);
  // Picks pieces with focus shown in the panel, as the panel's list does.
  void SetPick(const QStringList& picked, const QString& focus);
  // Doll space to widget pixels.
  QTransform World() const;

 signals:
  // The picked pieces and which one the panel shows.
  void PickChanged(const QStringList& picked, const QString& focus);
  // What the canvas is waiting for; empty when nothing.
  void Hint(const QString& hint);
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  enum class Drag { kNone, kPivot, kTip };

  void PaintOverlay(QPainter* painter) const;
  bool PressHandle(QPointF point);
  // The piece whose joint is under point, or empty.
  QString JointAt(QPointF point) const;
  // Ends hang mode, telling the window.
  void StopHanging();
  void ApplyPick(const std::set<QString>& pieces, PickMode mode,
                 const QString& focus);
  void FinishBox();

  Managers managers_;
  ImageCache cache_;
  QString doll_;
  QString piece_;
  std::set<QString> picked_;
  QPointF last_;
  bool is_boxing_ = false;
  QPointF box_from_;
  QPointF box_to_;
  PickMode box_mode_ = PickMode::kReplace;
  Drag drag_ = Drag::kNone;
  // Set after a joint is double-clicked: the next click picks its
  // parent. The line follows the cursor meanwhile.
  bool is_hanging_ = false;
  QPointF cursor_;
  QString chain_;
  std::unique_ptr<EditScope> scope_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_RIG_CANVAS_H_
