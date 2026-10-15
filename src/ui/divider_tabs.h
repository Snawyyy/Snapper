#ifndef SNAPPER_UI_DIVIDER_TABS_H_
#define SNAPPER_UI_DIVIDER_TABS_H_

#include <QMargins>
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

// A QTabWidget whose bar is a DividerTabBar and whose page is drawn as
// the open divider's sheet of paper lying on the others: one piece with
// its tab, the sheets under it peeking out at the right and bottom, and
// its shadow on them. Pages should leave PageMargins() clear so their
// contents stay on the sheet.
class DividerTabs final : public QTabWidget {
  Q_OBJECT

 public:
  explicit DividerTabs(QWidget* parent);

  static QMargins PageMargins();

 protected:
  void paintEvent(QPaintEvent* event) override;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DIVIDER_TABS_H_
