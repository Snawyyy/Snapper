#ifndef SNAPPER_UI_RIG_CANVAS_H_
#define SNAPPER_UI_RIG_CANVAS_H_

#include <QString>
#include <QTransform>
#include <QWidget>

#include <memory>

#include "edit/edit_scope.h"
#include "render/image_cache.h"
#include "ui/managers.h"

namespace snapper {

// A doll at rest, for jointing it. Click a piece to pick it; drag its
// yellow joint to move the pivot; drag a square IK tip to set where the
// chain reaches from. Lines join each joint to its parent's. Escape
// cancels a drag.
class RigCanvas final : public QWidget {
  Q_OBJECT

 public:
  explicit RigCanvas(const Managers& managers);

  void SetDoll(const QString& doll);
  void SetPiece(const QString& piece);
  // Doll space to widget pixels.
  QTransform World() const;

 signals:
  void PiecePicked(const QString& piece);
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  enum class Drag { kNone, kPivot, kTip };

  void PaintOverlay(QPainter* painter) const;
  bool PressHandle(QPointF point);

  Managers managers_;
  ImageCache cache_;
  QString doll_;
  QString piece_;
  Drag drag_ = Drag::kNone;
  QString chain_;
  std::unique_ptr<EditScope> scope_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_RIG_CANVAS_H_
