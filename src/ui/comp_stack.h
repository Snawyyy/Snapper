#ifndef SNAPPER_UI_COMP_STACK_H_
#define SNAPPER_UI_COMP_STACK_H_

#include <QMenu>
#include <QPointF>
#include <QPushButton>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <vector>

#include "model/comp.h"
#include "ui/managers.h"

namespace snapper {

// The Comp tab's effect stack: a "+" button that opens the kinds (A),
// then one row per effect, the top one drawn last: an eye, a kind mark,
// the name, the target and a small clock when it costs more than a
// frame. Click picks, the eye turns one off, drag reorders (one undo
// step, Escape cancels), right click duplicates or removes.
class CompStack final : public QWidget {
  Q_OBJECT

 public:
  explicit CompStack(const Managers& managers);

  QSize sizeHint() const override { return QSize(230, 320); }
  // Opens the Add menu under the "+" button.
  void OpenAdd();

 signals:
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void Report(const QString& problem) {
    if (!problem.isEmpty()) {
      emit Problem(problem);
    }
  }
  void BuildAddMenu();
  void Refresh();
  // The effects, top of the stack first.
  std::vector<CompEffect> Rows() const;
  double Top() const;
  QRectF RowRect(int row) const;
  QRectF EyeRect(int row) const;
  // The row under y, or -1; the gap a drop at y lands in (0 to count).
  int RowAt(double y) const;
  int GapAt(double y) const;

  Managers managers_;
  QPushButton add_;
  QMenu add_menu_;
  int pressed_row_ = -1;
  QPointF press_at_;
  bool is_dragging_ = false;
  int drop_gap_ = -1;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_STACK_H_
