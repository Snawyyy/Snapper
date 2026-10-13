#ifndef SNAPPER_UI_DOCK_GRIPS_H_
#define SNAPPER_UI_DOCK_GRIPS_H_

#include <QElapsedTimer>
#include <QObject>
#include <QRect>
#include <QString>
#include <QTimer>

#include <vector>

class QDockWidget;
class QEvent;
class QMainWindow;
class QPoint;

namespace snapper {

// The separators between a window's side docks and its centre, which
// Qt draws and drags itself but gives no hover state or double click:
// this lights the one under the pointer once it has rested there for
// kGripDwellMs (at once while dragged), keeps the widths the docks were
// dragged to in the settings file, and puts a dock back to its default
// width on a double click. The style paints the lit one from the
// window's "litGrip" property.
class DockGrips final : public QObject {
  Q_OBJECT

 public:
  // Docks are named by their object names, which must be set. An empty
  // settings_path keeps nothing.
  DockGrips(QMainWindow* window, std::vector<QDockWidget*> docks,
            const QString& settings_path);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  // The separator under at (window pixels) with the dock it belongs to,
  // or an empty rect.
  QRect SeparatorAt(const QPoint& at, QDockWidget** dock) const;
  // Follows the pointer: lights, dims and publishes the lit separator.
  void Follow();
  void Publish(bool is_lit);
  void RestoreWidths();
  void SaveWidths() const;
  void ResetWidth(QDockWidget* dock);
  int DefaultWidth() const;

  QMainWindow* window_;
  std::vector<QDockWidget*> docks_;
  QString settings_path_;
  // The separator the pointer is on, since when, and whether lit.
  QRect over_;
  QElapsedTimer rested_;
  bool is_lit_ = false;
  bool is_down_ = false;
  // Runs only while a separator is under the pointer, to see it leave.
  QTimer tick_;
  bool is_restored_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DOCK_GRIPS_H_
