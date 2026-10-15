#ifndef SNAPPER_UI_DIVIDER_TABS_H_
#define SNAPPER_UI_DIVIDER_TABS_H_

#include <QSize>
#include <QTabBar>
#include <QTabWidget>

class QPaintEvent;

namespace snapper {

// Tabs drawn as the tabs of stacked index page dividers: rectangles
// that reach over their neighbours, each sheet further from the open
// one lying further back, darker and in the shadow of the one in front.
// The open one keeps the panel's colour and runs into a strip of it
// under the bar like its sheet; it is never lightened.
class DividerTabBar final : public QTabBar {
  Q_OBJECT

 public:
  explicit DividerTabBar(QWidget* parent);

 protected:
  QSize tabSizeHint(int index) const override;
  void paintEvent(QPaintEvent* event) override;

 private:
  // depth: 0 for the open tab, else how many sheets behind it.
  void PaintTab(QPainter* painter, int index, int depth) const;
};

// A QTabWidget whose bar is a DividerTabBar.
class DividerTabs final : public QTabWidget {
  Q_OBJECT

 public:
  explicit DividerTabs(QWidget* parent);
};

}  // namespace snapper

#endif  // SNAPPER_UI_DIVIDER_TABS_H_
