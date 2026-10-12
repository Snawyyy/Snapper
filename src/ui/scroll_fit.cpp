#include "ui/scroll_fit.h"

#include <QEvent>
#include <QScrollArea>
#include <QScrollBar>

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
    area_->setMinimumWidth(inside->minimumSizeHint().width() + bar + frame);
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
  QScrollArea* area_;
};

}  // namespace

void KeepWideEnough(QScrollArea* area) {
  assert(area != nullptr);
  assert(area->widget() != nullptr);
  auto* follower = new WidthFollower(area);
  area->widget()->installEventFilter(follower);
  follower->Fit();
}

}  // namespace snapper
