#ifndef SNAPPER_UI_TOAST_VIEW_H_
#define SNAPPER_UI_TOAST_VIEW_H_

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

#include "ui/toast_queue.h"

namespace snapper {

class MotionManager;
class ToastManager;

// One toast on screen: its text, an optional Undo button and a thin
// bar that runs down with its time. It never takes focus.
class ToastItem final : public QFrame {
  Q_OBJECT

 public:
  ToastItem(const Toast& toast, QWidget* parent);

  int id() const { return id_; }
  // Shows the toast's current state: time left, Undo or not.
  void Update(const Toast& toast);

 signals:
  void Clicked(int id);
  void UndoPressed(int id);

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  int id_;
  int left_ms_;
  QHBoxLayout layout_;
  QLabel label_;
  QPushButton undo_;
};

// Draws the toasts of a ToastManager in a column above the bottom edge
// of an anchor widget. It floats over the window, so showing or hiding
// a toast moves nothing, and only the toasts themselves catch the
// mouse. Escape takes the newest away.
class ToastView final : public QWidget {
  Q_OBJECT

 public:
  // host holds the view; anchor is the area it sits at the bottom
  // centre of.
  ToastView(ToastManager* toasts, MotionManager* motion, QWidget* host,
            QWidget* anchor);

  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  // Brings the items in line with the manager's queue.
  void Sync();
  void AddItem(const Toast& toast);
  void Place();
  void Fade(ToastItem* item, double from, double to, bool is_leaving);

  ToastManager* toasts_;
  MotionManager* motion_;
  QWidget* host_;
  QWidget* anchor_;
  QVBoxLayout layout_;
  std::vector<QPointer<ToastItem>> items_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TOAST_VIEW_H_
