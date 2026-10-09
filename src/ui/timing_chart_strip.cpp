#include "ui/timing_chart_strip.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "edit/timing_chart_manager.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kMargin = 30.0;
// Ticks look thin and grab wide.
constexpr double kGrabPx = 9.0;
constexpr double kLineY = 34.0;
constexpr double kTickHalf = 9.0;
constexpr double kLabelMinPx = 24.0;

}  // namespace

TimingChartStrip::TimingChartStrip(TimingChartManager* chart)
    : chart_(chart) {
  assert(chart_ != nullptr);
  setMouseTracking(true);
  setFocusPolicy(Qt::ClickFocus);
  assert(hasMouseTracking());
  connect(chart_, &TimingChartManager::Changed, this,
          qOverload<>(&QWidget::update));
}

QSize TimingChartStrip::sizeHint() const { return QSize(460, 78); }
QSize TimingChartStrip::minimumSizeHint() const { return QSize(300, 78); }

double TimingChartStrip::XOf(double fraction) const {
  assert(width() > 0);
  assert(fraction >= 0.0 && fraction <= 1.0);
  return kMargin + fraction * (width() - 2.0 * kMargin);
}

double TimingChartStrip::FractionAt(double x) const {
  assert(width() > 0);
  const double line = std::max(1.0, width() - 2.0 * kMargin);
  assert(line > 0.0);
  return (x - kMargin) / line;
}

int TimingChartStrip::PartAt(double x) const {
  assert(chart_ != nullptr);
  assert(width() > 0);
  const auto& fractions = chart_->fractions();
  const int count = static_cast<int>(fractions.size());
  int best = -2;
  double best_gap = kGrabPx;
  for (int i = -1; i <= count; ++i) {
    const bool is_end = i < 0 || i == count;
    const double at = is_end ? (i < 0 ? 0.0 : 1.0)
                             : fractions[static_cast<size_t>(i)];
    const double gap = std::abs(XOf(at) - x);
    const bool is_closer = gap <= best_gap;
    if (is_closer) {
      best = i;
      best_gap = gap;
    }
  }
  return best;
}

int TimingChartStrip::TickAt(double x) const {
  assert(chart_ != nullptr);
  assert(width() > 0);
  const int part = PartAt(x);
  const bool is_tick =
      part >= 0 && part < static_cast<int>(chart_->fractions().size());
  return is_tick ? part : -1;
}

void TimingChartStrip::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(chart_ != nullptr);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), theme::kField);
  const bool is_open = chart_->IsOpen();
  if (!is_open) {
    return;
  }
  painter.setPen(QPen(theme::kTextOff, 2));
  painter.drawLine(QPointF(XOf(0.0), kLineY), QPointF(XOf(1.0), kLineY));
  PaintTicks(&painter);
  PaintEnds(&painter);
}

void TimingChartStrip::PaintTicks(QPainter* painter) const {
  assert(painter != nullptr);
  const auto& fractions = chart_->fractions();
  assert(fractions.size() <= static_cast<size_t>(kMaxChartFrames));
  const double slot = (XOf(1.0) - XOf(0.0)) /
                      static_cast<double>(fractions.size() + 1);
  const int stride = std::max(1, static_cast<int>(std::ceil(
                                     kLabelMinPx / std::max(slot, 1.0))));
  QFont font = painter->font();
  font.setPointSize(theme::kFontPoints - 2);
  painter->setFont(font);
  for (int i = 0; i < static_cast<int>(fractions.size()); ++i) {
    const double x = XOf(fractions[static_cast<size_t>(i)]);
    const bool is_live = i == dragged_ || i == hovered_;
    painter->setPen(QPen(is_live ? theme::kPick : theme::kHandle,
                         is_live ? 3 : 2));
    painter->drawLine(QPointF(x, kLineY - kTickHalf),
                      QPointF(x, kLineY + kTickHalf));
    const int frame = chart_->frames()[static_cast<size_t>(i)];
    const bool has_label = is_live || i % stride == 0;
    if (has_label) {
      painter->setPen(theme::kText);
      painter->drawText(QRectF(x - 14, kLineY + kTickHalf + 2, 28, 14),
                        Qt::AlignHCenter,
                        QString::number(frame));
    }
  }
}

void TimingChartStrip::PaintEnds(QPainter* painter) const {
  assert(painter != nullptr);
  assert(chart_->IsOpen());
  const int count = static_cast<int>(chart_->fractions().size());
  const struct {
    double at;
    const char* name;
    int frame;
    int part;
  } ends[] = {{0.0, "A", chart_->a().index(), -1},
              {1.0, "B", chart_->b().index(), count}};
  for (const auto& end : ends) {
    const double x = XOf(end.at);
    const bool is_live = hovered_ == end.part;
    painter->setPen(QPen(is_live ? theme::kPick : theme::kText, 3));
    painter->drawLine(QPointF(x, kLineY - 16), QPointF(x, kLineY + 16));
    painter->setPen(theme::kText);
    painter->drawText(QRectF(x - 14, 4, 28, 14), Qt::AlignHCenter,
                      QString::fromLatin1(end.name));
    painter->drawText(QRectF(x - 14, kLineY + 20, 28, 14), Qt::AlignHCenter,
                      QString::number(end.frame));
  }
}

void TimingChartStrip::Hover(double x) {
  assert(chart_ != nullptr);
  assert(dragged_ < 0);
  const int part = PartAt(x);
  const bool is_new = part != hovered_;
  if (!is_new) {
    return;
  }
  hovered_ = part;
  setCursor(part >= 0 && part < static_cast<int>(chart_->fractions().size())
                ? Qt::SizeHorCursor
                : Qt::ArrowCursor);
  const bool is_part = part > -2;
  if (is_part) {
    chart_->ShowTick(part);
  }
  update();
}

void TimingChartStrip::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(chart_ != nullptr);
  const int tick = TickAt(event->position().x());
  const bool is_grab = event->button() == Qt::LeftButton && tick >= 0;
  if (!is_grab) {
    return;
  }
  dragged_ = tick;
  chart_->BeginTickDrag();
  chart_->ShowTick(tick);
  setFocus();
}

void TimingChartStrip::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(chart_ != nullptr);
  const bool is_dragging = dragged_ >= 0;
  if (!is_dragging) {
    Hover(event->position().x());
    return;
  }
  const auto moved =
      chart_->MoveTick(dragged_, FractionAt(event->position().x()));
  const bool has_moved = moved.has_value();
  if (has_moved) {
    emit Edited();
    chart_->ShowTick(dragged_);
  }
}

void TimingChartStrip::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(chart_ != nullptr);
  const bool was_dragging = dragged_ >= 0;
  if (was_dragging) {
    chart_->EndTickDrag();
    hovered_ = dragged_;
    dragged_ = -1;
    update();
  }
}

void TimingChartStrip::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(chart_ != nullptr);
  const bool is_cancel = event->key() == Qt::Key_Escape && dragged_ >= 0;
  if (!is_cancel) {
    QWidget::keyPressEvent(event);
    return;
  }
  // The start was a valid chart a moment ago, so putting it back fits.
  const bool is_restored = chart_->CancelTickDrag().has_value();
  assert(is_restored);
  static_cast<void>(is_restored);
  dragged_ = -1;
  releaseMouse();
  emit Edited();
  event->accept();
  update();
}

void TimingChartStrip::leaveEvent(QEvent* event) {
  assert(event != nullptr);
  assert(hovered_ >= -2);
  const bool is_hovering = hovered_ != -2 && dragged_ < 0;
  if (is_hovering) {
    hovered_ = -2;
    update();
  }
  QWidget::leaveEvent(event);
}

}  // namespace snapper
