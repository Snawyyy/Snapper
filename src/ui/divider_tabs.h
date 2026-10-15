#ifndef SNAPPER_UI_DIVIDER_TABS_H_
#define SNAPPER_UI_DIVIDER_TABS_H_

#include <QSize>
#include <QTabBar>
#include <QTabWidget>

class QPaintEvent;

namespace snapper {

// Tabs drawn as the coloured tabs of index page dividers: each its own
// colour, slanted at the sides. The open one shows its full colour and
// a strip of it runs under the bar like the divider's sheet; the rest
// are darkened.
class DividerTabBar final : public QTabBar {
  Q_OBJECT

 public:
  explicit DividerTabBar(QWidget* parent);

 protected:
  QSize tabSizeHint(int index) const override;
  void paintEvent(QPaintEvent* event) override;

 private:
  void PaintTab(QPainter* painter, int index) const;
};

// A QTabWidget whose bar is a DividerTabBar.
class DividerTabs final : public QTabWidget {
  Q_OBJECT

 public:
  explicit DividerTabs(QWidget* parent);
};

}  // namespace snapper

#endif  // SNAPPER_UI_DIVIDER_TABS_H_
