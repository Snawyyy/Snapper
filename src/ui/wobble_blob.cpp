#include "ui/wobble_blob.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>

#include <cassert>
#include <vector>

#include "base/frame.h"
#include "ui/motion.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// One preview frame per stage frame, so the hop takes as long as it
// would on the stage.
constexpr int kFrameMs = 1000 / kFramesPerSecond;
// How far the floor sits above the widget's bottom edge.
constexpr double kFloorMargin = 8.0;

// The blob's edge through dots, smoothed: a curve through the middle
// of each side, bent at each dot.
QPainterPath EdgeOf(const std::vector<QPointF>& dots, QPointF origin) {
  const std::vector<int> outline = HopPreview::Outline();
  assert(outline.size() > 2);
  assert(!dots.empty());
  const auto at = [&](size_t k) {
    return origin + dots[static_cast<size_t>(outline[k % outline.size()])];
  };
  QPainterPath path((at(0) + at(1)) / 2.0);
  for (size_t k = 1; k <= outline.size(); ++k) {
    path.quadTo(at(k), (at(k) + at(k + 1)) / 2.0);
  }
  path.closeSubpath();
  return path;
}

}  // namespace

WobbleBlob::WobbleBlob(MotionManager* motion, QWidget* parent)
    : QWidget(parent), motion_(motion) {
  assert(motion != nullptr);
  assert(kFrameMs > 0);
  setObjectName("feel_preview");
  setFixedSize(sizeHint());
  timer_.setInterval(kFrameMs);
  connect(&timer_, &QTimer::timeout, this, [this] {
    hop_.Step();
    update();
  });
  connect(motion_, &MotionManager::ModeChanged, this,
          [this] { Sync(); });
}

QSize WobbleBlob::sizeHint() const {
  assert(kFrameMs > 0);
  assert(kFloorMargin > 0.0);
  return {96, 80};
}

void WobbleBlob::SetNode(const DragNode* node) {
  assert(motion_ != nullptr);
  assert(node == nullptr || node->kind == NodeKind::kDrag ||
         node->kind == NodeKind::kCloth || node->kind == NodeKind::kJelly);
  hop_.SetNode(node);
  update();
}

void WobbleBlob::Sync() {
  assert(motion_ != nullptr);
  assert(kFrameMs > 0);
  const bool is_moving = isVisible() && isEnabled() &&
                         motion_->mode() != MotionMode::kOff;
  if (is_moving) {
    timer_.start();
    return;
  }
  timer_.stop();
  hop_.Rest();
  update();
}

void WobbleBlob::showEvent(QShowEvent* event) {
  assert(event != nullptr);
  assert(motion_ != nullptr);
  QWidget::showEvent(event);
  Sync();
}

void WobbleBlob::hideEvent(QHideEvent* event) {
  assert(event != nullptr);
  assert(motion_ != nullptr);
  QWidget::hideEvent(event);
  Sync();
}

void WobbleBlob::changeEvent(QEvent* event) {
  assert(event != nullptr);
  assert(motion_ != nullptr);
  QWidget::changeEvent(event);
  const bool is_enable = event->type() == QEvent::EnabledChange;
  if (is_enable) {
    Sync();
  }
}

void WobbleBlob::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(motion_ != nullptr);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  const QRectF box = QRectF(rect()).adjusted(1, 1, -1, -1);
  painter.fillRect(box, theme::kField);
  const QPointF origin(box.center().x(), box.bottom() - kFloorMargin);
  const QColor ink = isEnabled() ? theme::kText : theme::kTextOff;
  painter.setPen(QPen(theme::kTextOff, 1.0));
  painter.drawLine(QPointF(box.left() + 6, origin.y()),
                   QPointF(box.right() - 6, origin.y()));
  painter.setPen(QPen(ink, 1.5));
  painter.setBrush(isEnabled() ? theme::kPick : theme::kFace);
  painter.drawPath(EdgeOf(hop_.dots(), origin));
  const int spring = hop_.spring_dot();
  const bool has_spring = spring >= 0;
  if (has_spring) {
    painter.setBrush(ink);
    painter.drawEllipse(origin + hop_.dots()[static_cast<size_t>(spring)],
                        3.5, 3.5);
  }
}

}  // namespace snapper
