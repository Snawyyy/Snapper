#ifndef SNAPPER_UI_TOOL_BUTTON_H_
#define SNAPPER_UI_TOOL_BUTTON_H_

#include <QString>
#include <QToolButton>

namespace snapper {

// A tool on a tool bar: its name with its key under it in small print,
// and, while it is the tool in use, a raised plate with a Teto red strip
// along its foot, so the tool in hand is never in doubt.
class ToolButton final : public QToolButton {
  Q_OBJECT

 public:
  ToolButton(const QString& name, const QString& key,
             QWidget* parent = nullptr);

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  QString name_;
  QString key_;
  bool is_hovered_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TOOL_BUTTON_H_
