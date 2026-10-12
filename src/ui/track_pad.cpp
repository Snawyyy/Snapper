#include "ui/track_pad.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>

#include <cassert>
#include <cmath>

#include "ui/theme.h"

namespace snapper {
namespace {

// Grid lines are this many pixels apart on the pad.
constexpr double kGridGap = 12.0;
constexpr double kPuckRadius = 5.0;
constexpr double kFineDivisor = 10.0;

// How far value slides the grid: the grid moves with the drag, so a
// positive value shifts it the same way the hand went.
double Slide(double value) {
  assert(std::isfinite(value));
  assert(kGridGap > 0.0);
  const double wrapped = std::fmod(value, kGridGap);
  return wrapped < 0.0 ? wrapped + kGridGap : wrapped;
}

}  // namespace

TrackPad::TrackPad(QDoubleSpinBox* x, QDoubleSpinBox* y, LiveEdit* live,
                   QWidget* parent)
    : QWidget(parent), x_(x), y_(y), drag_({x, y}, live) {
  assert(x_ != nullptr && y_ != nullptr);
  assert(sizeHint().width() > 0);
  setFixedHeight(sizeHint().height());
  setMinimumWidth(sizeHint().width());
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setFocusPolicy(Qt::StrongFocus);
  setCursor(Qt::SizeAllCursor);
  setToolTip(tr("Drag to move. Shift for fine, double click to centre, "
                "Escape cancels."));
  for (const QDoubleSpinBox* box : {x_, y_}) {
    const QLineEdit* text = box->findChild<QLineEdit*>();
    assert(text != nullptr);
    connect(text, &QLineEdit::textChanged, this, [this] { update(); });
  }
}

void TrackPad::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(width() > 0);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const bool is_on = x_->isEnabled();
  const QRectF pad(0.5, 0.5, width() - 1.0, height() - 1.0);
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(theme::kField);
  painter.drawRect(pad);
  painter.setClipRect(pad.adjusted(1, 1, -1, -1));
  QColor line = theme::kFaceLight;
  line.setAlpha(is_on ? 110 : 50);
  painter.setPen(QPen(line, 1.0));
  const double slide_x = Slide(x_->value());
  const double slide_y = Slide(y_->value());
  for (double gx = slide_x; gx < width(); gx += kGridGap) {
    painter.drawLine(QPointF(gx, 0), QPointF(gx, height()));
  }
  for (double gy = slide_y; gy < height(); gy += kGridGap) {
    painter.drawLine(QPointF(0, gy), QPointF(width(), gy));
  }
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(is_on ? theme::kHandle : theme::kTextOff);
  const double grow = drag_.is_active() ? 1.5 : 0.0;
  painter.drawEllipse(pad.center(), kPuckRadius + grow, kPuckRadius + grow);
}

void TrackPad::Drag(const QMouseEvent* event) {
  assert(event != nullptr);
  assert(drag_.is_active());
  const bool is_fine = event->modifiers().testFlag(Qt::ShiftModifier);
  const QPointF moved =
      (event->position() - press_) / (is_fine ? kFineDivisor : 1.0);
  drag_.Set(0, drag_.start(0) + moved.x());
  drag_.Set(1, drag_.start(1) + moved.y());
  update();
}

void TrackPad::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(!drag_.is_active());
  const bool can_drag =
      event->button() == Qt::LeftButton && drag_.is_enabled();
  if (!can_drag) {
    QWidget::mousePressEvent(event);
    return;
  }
  drag_.Begin();
  press_ = event->position();
  setFocus();
  grabKeyboard();
}

void TrackPad::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(std::isfinite(press_.x()));
  const bool is_dragging = drag_.is_active();
  if (is_dragging) {
    Drag(event);
  }
}

void TrackPad::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(std::isfinite(press_.x()));
  releaseKeyboard();
  drag_.Finish();
  update();
}

void TrackPad::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(!drag_.is_active());
  const bool can_centre = drag_.is_enabled();
  if (can_centre) {
    drag_.SetAll({0.0, 0.0});
  }
}

void TrackPad::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(x_ != nullptr);
  const bool is_cancel = event->key() == Qt::Key_Escape && drag_.is_active();
  if (!is_cancel) {
    QWidget::keyPressEvent(event);
    return;
  }
  releaseKeyboard();
  drag_.Cancel();
  update();
}

}  // namespace snapper
