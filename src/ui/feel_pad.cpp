#include "ui/feel_pad.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kPadSize = 132;
constexpr int kLabelBand = 14;
constexpr double kPuckRadius = 7.0;

}  // namespace

Feel FeelAt(const QPointF& point, const QRectF& area) {
  assert(area.width() > 0.0 && area.height() > 0.0);
  assert(std::isfinite(point.x() + point.y()));
  const double x = (point.x() - area.left()) / area.width();
  const double y = (point.y() - area.top()) / area.height();
  return {std::clamp(x, 0.0, 1.0), std::clamp(1.0 - y, 0.0, 1.0)};
}

QPointF PointOf(const Feel& feel, const QRectF& area) {
  assert(area.width() > 0.0 && area.height() > 0.0);
  assert(std::isfinite(feel.firm + feel.heavy));
  return {area.left() + area.width() * feel.firm,
          area.top() + area.height() * (1.0 - feel.heavy)};
}

FeelPad::FeelPad(QWidget* parent) : QWidget(parent) {
  assert(kPadSize > kLabelBand);
  setObjectName("feel_pad");
  setFocusPolicy(Qt::ClickFocus);
  setCursor(Qt::CrossCursor);
  // As wide as it is given, so it can take its share of a row.
  setFixedHeight(sizeHint().height());
  setMinimumWidth(sizeHint().width());
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  assert(Area().isValid());
}

QSize FeelPad::sizeHint() const {
  assert(kPadSize > 0);
  assert(kLabelBand > 0);
  return {kPadSize, kPadSize + kLabelBand};
}

QRectF FeelPad::Area() const {
  assert(kLabelBand > 0);
  assert(width() >= 0);
  return QRectF(0, 0, width(), height() - kLabelBand)
      .adjusted(1, 1, -1, -1);
}

void FeelPad::SetFeels(const std::vector<Feel>& feels) {
  assert(feels.size() < 1000);
  assert(feels_.size() < 1000);
  const bool is_held = is_dragging_;
  if (is_held) {
    return;
  }
  feels_ = feels;
  update();
}

void FeelPad::Drag(const QPointF& point) {
  assert(is_dragging_);
  assert(Area().isValid());
  dragged_ = FeelAt(point, Area());
  feels_ = {dragged_};
  emit Moved(dragged_);
  update();
}

void FeelPad::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(Area().isValid());
  const bool is_left = event->button() == Qt::LeftButton;
  if (!is_left) {
    return;
  }
  is_dragging_ = true;
  setFocus();
  emit Grabbed();
  Drag(event->position());
}

void FeelPad::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(Area().isValid());
  if (is_dragging_) {
    Drag(event->position());
  }
}

void FeelPad::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(Area().isValid());
  const bool is_end = is_dragging_ && event->button() == Qt::LeftButton;
  if (is_end) {
    is_dragging_ = false;
    emit Released();
  }
}

void FeelPad::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(Area().isValid());
  const bool is_cancel = is_dragging_ && event->key() == Qt::Key_Escape;
  if (is_cancel) {
    is_dragging_ = false;
    emit Cancelled();
    return;
  }
  QWidget::keyPressEvent(event);
}

void FeelPad::PaintPucks(QPainter* painter) const {
  assert(painter != nullptr);
  assert(Area().isValid());
  const bool is_one = feels_.size() == 1;
  for (const Feel& feel : feels_) {
    const QPointF at = PointOf(feel, Area());
    painter->setPen(QPen(is_one ? theme::kPick : theme::kHandle, 2.0));
    painter->setBrush(is_one ? QBrush(theme::kHandle) : Qt::NoBrush);
    painter->drawEllipse(at, kPuckRadius, kPuckRadius);
  }
  const bool is_chip = is_dragging_;
  if (is_chip) {
    const QPointF at = PointOf(dragged_, Area());
    const QString text = tr("firm %1  heavy %2")
                             .arg(dragged_.firm, 0, 'f', 2)
                             .arg(dragged_.heavy, 0, 'f', 2);
    const QRectF chip(at.x() - 50, at.y() - 26, 100, 16);
    painter->fillRect(chip, theme::kFaceDark);
    painter->setPen(theme::kText);
    painter->drawText(chip, Qt::AlignCenter, text);
  }
}

void FeelPad::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(Area().isValid());
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  const QRectF area = Area();
  const QColor ink = isEnabled() ? theme::kText : theme::kTextOff;
  painter.fillRect(area, theme::kField);
  painter.setPen(theme::kFaceLight);
  painter.drawLine(QLineF(area.center().x(), area.top(), area.center().x(),
                          area.bottom()));
  painter.drawLine(QLineF(area.left(), area.center().y(), area.right(),
                          area.center().y()));
  painter.setPen(theme::kShadow);
  painter.drawRect(area);
  painter.setPen(ink);
  painter.drawText(area.adjusted(3, 2, 0, 0),
                   Qt::AlignLeft | Qt::AlignTop, tr("heavy"));
  painter.drawText(area.adjusted(3, 0, 0, -2),
                   Qt::AlignLeft | Qt::AlignBottom, tr("light"));
  const QRectF band(0, area.bottom() + 1, width(), kLabelBand);
  painter.drawText(band, Qt::AlignLeft | Qt::AlignVCenter, tr("soft"));
  painter.drawText(band, Qt::AlignRight | Qt::AlignVCenter, tr("firm"));
  const bool is_live = isEnabled();
  if (is_live) {
    PaintPucks(&painter);
  }
}

}  // namespace snapper
