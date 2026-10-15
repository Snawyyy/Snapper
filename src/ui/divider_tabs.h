#ifndef SNAPPER_UI_DIVIDER_TABS_H_
#define SNAPPER_UI_DIVIDER_TABS_H_

#include <QSize>
#include <QTabBar>
#include <QTabWidget>

class QPaintEvent;

namespace snapper {

// Tabs drawn as the tabs of index page dividers: plain rectangles with
// a gap between. The open one keeps the panel's colour and runs into a
// strip of it under the bar like the divider's sheet; the rest are
// darkened, never the open one lightened.
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
