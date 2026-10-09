#ifndef SNAPPER_UI_MODE_TABS_H_
#define SNAPPER_UI_MODE_TABS_H_

#include <QPointer>
#include <QRectF>
#include <QTabBar>
#include <QVariantAnimation>

class QPaintEvent;

namespace snapper {

class MotionManager;

// The Pose, Rig and Video tabs: chunky tabs from SnapperStyle with a
// Teto red bar under the active one that slides to the next when the
// tab changes (the Move time; it jumps in Off).
class ModeTabs final : public QTabBar {
  Q_OBJECT

 public:
  explicit ModeTabs(MotionManager* motion);

  // Where the red bar is now; sliding while it moves.
  QRectF bar() const { return bar_; }

 protected:
  void paintEvent(QPaintEvent* event) override;
  void tabLayoutChange() override;
  void tabInserted(int index) override;

 private:
  // Draws each tab's icon, red on the picked one.
  void RefreshIcons();
  // Slides the bar under the tab at index.
  void SlideTo(int index);
  QRectF BarUnder(int index) const;

  MotionManager* motion_;
  QRectF bar_;
  QPointer<QVariantAnimation> animation_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_MODE_TABS_H_
