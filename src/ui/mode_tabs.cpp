#include "ui/mode_tabs.h"

#include <QPainter>

#include <cassert>

#include "ui/motion.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kBarHeightPx = 3;

}  // namespace

ModeTabs::ModeTabs(MotionManager* motion) : motion_(motion) {
  assert(motion != nullptr);
  assert(count() == 0);
  setExpanding(false);
  setDrawBase(false);
  setFocusPolicy(Qt::NoFocus);
  connect(this, &QTabBar::currentChanged, this, &ModeTabs::SlideTo);
}

QRectF ModeTabs::BarUnder(int index) const {
  assert(index >= -1);
  assert(index < count());
  const bool has_tab = index >= 0;
  if (!has_tab) {
    return QRectF();
  }
  const QRect tab = tabRect(index);
  return QRectF(tab.left() + 1, tab.bottom() - kBarHeightPx + 1,
                tab.width() - 4, kBarHeightPx);
}

void ModeTabs::SlideTo(int index) {
  assert(motion_ != nullptr);
  assert(index < count());
  const bool is_running = !animation_.isNull();
  if (is_running) {
    animation_->stop();
  }
  const QRectF to = BarUnder(index);
  const bool is_first = bar_.isNull() || to.isNull();
  if (is_first) {
    bar_ = to;
    update();
    return;
  }
  animation_ = motion_->Animate(this, MotionKind::kMove, bar_, to,
                                [this](const QVariant& value) {
                                  bar_ = value.toRectF();
                                  update();
                                });
}

void ModeTabs::tabLayoutChange() {
  QTabBar::tabLayoutChange();
  assert(count() >= 0);
  assert(currentIndex() < count());
  const bool is_sliding = !animation_.isNull();
  if (!is_sliding) {
    bar_ = BarUnder(currentIndex());
  }
}

void ModeTabs::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(bar_.width() >= 0.0);
  QTabBar::paintEvent(event);
  QPainter painter(this);
  painter.fillRect(bar_, theme::kPick);
}

}  // namespace snapper
