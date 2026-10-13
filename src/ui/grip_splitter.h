#ifndef SNAPPER_UI_GRIP_SPLITTER_H_
#define SNAPPER_UI_GRIP_SPLITTER_H_

#include <QList>
#include <QSplitter>
#include <QSplitterHandle>
#include <QString>
#include <QTimer>

namespace snapper {

// A splitter handle that lights up yellow once the pointer has rested on
// it for kGripDwellMs, or at once while it is dragged. It follows enter
// and leave itself, so it does not depend on the style's hover events.
// A double click on it asks for the default sizes.
class GripHandle final : public QSplitterHandle {
  Q_OBJECT

 public:
  GripHandle(Qt::Orientation orientation, QSplitter* parent);

 signals:
  // The mouse let go after a drag.
  void Released();
  void DoubleClicked();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;

 private:
  QTimer dwell_;
  bool is_lit_ = false;
  bool is_down_ = false;
};

// A splitter whose handles are GripHandles: wide to grab, lit when
// rested on, a double click puts the sizes back to the default. It can
// keep its sizes in the settings file between runs.
class GripSplitter final : public QSplitter {
  Q_OBJECT

 public:
  explicit GripSplitter(Qt::Orientation orientation,
                        QWidget* parent = nullptr);

  // The shares the panes take by default, one per pane (also their
  // stretch factors); a 0 is a pane that keeps the size it asks for.
  void SetDefaultShares(const QList<int>& shares);
  // Keeps the sizes under key in the settings file at settings_path:
  // put back when first shown, saved when a handle is let go. An empty
  // path keeps nothing.
  void Remember(const QString& key, const QString& settings_path);
  // The default sizes again, and the saved ones forgotten.
  void ResetSizes();

 protected:
  QSplitterHandle* createHandle() override;
  void showEvent(QShowEvent* event) override;

 private:
  QList<int> DefaultSizes(int total) const;
  void SaveSizes() const;
  void RestoreSizes();

  QList<int> shares_;
  QString key_;
  QString settings_path_;
  bool is_restored_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_GRIP_SPLITTER_H_
