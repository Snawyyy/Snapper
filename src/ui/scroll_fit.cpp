#include "ui/scroll_fit.h"

#include <QEvent>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>

#include <algorithm>
#include <cassert>

namespace snapper {
namespace {

// Watches the scrolled widget and widens the area to it.
class WidthFollower final : public QObject {
 public:
  explicit WidthFollower(QScrollArea* area) : QObject(area), area_(area) {
    assert(area_ != nullptr);
  }

  void Fit() const {
    const QWidget* inside = area_->widget();
    if (inside == nullptr) {
      return;
    }
    const int bar = area_->verticalScrollBar()->sizeHint().width();
    const int frame = 2 * area_->frameWidth();
    const int needed = inside->minimumSizeHint().width() + bar + frame;
    area_->setMinimumWidth(CappedWidth(needed, ScreenWidth()));
  }

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    assert(event != nullptr);
    const bool is_relaid = event->type() == QEvent::LayoutRequest ||
                           event->type() == QEvent::Show;
    if (is_relaid && watched == area_->widget()) {
      Fit();
    }
    return QObject::eventFilter(watched, event);
  }

 private:
  // The width of the screen the area is on (0 when it has none yet).
  int ScreenWidth() const {
    assert(area_ != nullptr);
    const QScreen* screen = area_->screen();
    const bool has_screen = screen != nullptr;
    return has_screen ? screen->availableGeometry().width() : 0;
  }

  QScrollArea* area_;
};

}  // namespace

int CappedWidth(int needed, int screen_width) {
  assert(needed >= 0);
  assert(screen_width >= 0);
  const bool has_screen = screen_width > 0;
  if (!has_screen) {
    return needed;
  }
  const int cap = static_cast<int>(screen_width * kWidestShare);
  return std::min(needed, cap);
}

void KeepWideEnough(QScrollArea* area) {
  assert(area != nullptr);
  assert(area->widget() != nullptr);
  auto* follower = new WidthFollower(area);
  area->widget()->installEventFilter(follower);
  follower->Fit();
}

}  // namespace snapper
