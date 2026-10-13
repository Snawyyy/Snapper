#ifndef SNAPPER_UI_GRIP_SPLITTER_H_
#define SNAPPER_UI_GRIP_SPLITTER_H_

#include <QSplitter>
#include <QSplitterHandle>

namespace snapper {

// A splitter handle that lights up yellow while the pointer is over it
// or it is dragged. It follows enter and leave itself, so it does not
// depend on the style's hover events.
class GripHandle final : public QSplitterHandle {
  Q_OBJECT

 public:
  GripHandle(Qt::Orientation orientation, QSplitter* parent);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private:
  bool is_over_ = false;
  bool is_down_ = false;
};

// A splitter whose handles are GripHandles: wide to grab, lit when
// reached.
class GripSplitter final : public QSplitter {
  Q_OBJECT

 public:
  explicit GripSplitter(Qt::Orientation orientation,
                        QWidget* parent = nullptr);

 protected:
  QSplitterHandle* createHandle() override;
};

}  // namespace snapper

#endif  // SNAPPER_UI_GRIP_SPLITTER_H_
