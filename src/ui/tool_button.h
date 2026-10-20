#ifndef SNAPPER_UI_TOOL_BUTTON_H_
#define SNAPPER_UI_TOOL_BUTTON_H_

#include <QIcon>
#include <QString>
#include <QToolButton>

class QPainter;

namespace snapper {

// A tool on a tool bar: its name with its key under it in small print,
// and, while it is the tool in use, a raised plate with a Teto red strip
// along its foot, so the tool in hand is never in doubt. Given an icon it
// is a square for a tool box instead: the icon with the key small in its
// corner, the red strip down its left side.
class ToolButton final : public QToolButton {
  Q_OBJECT

 public:
  ToolButton(const QString& name, const QString& key,
             QWidget* parent = nullptr);
  ToolButton(const QIcon& icon, const QString& name, const QString& key,
             QWidget* parent = nullptr);

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  // The tool box look: the icon, the key in the corner, the strip down
  // the left.
  void PaintSquare(QPainter* painter, bool is_on, bool is_live);

  QString name_;
  QString key_;
  QIcon icon_;
  bool is_hovered_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TOOL_BUTTON_H_
