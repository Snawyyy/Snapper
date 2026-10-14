#include "ui/on_switch.h"

#include <QPainter>

#include <cassert>

#include "ui/theme.h"

namespace snapper {
namespace {

// The square, and the room round it that still takes the click.
constexpr int kSquare = 18;
constexpr int kRoom = 3;
constexpr int kInset = 4;

}  // namespace

OnSwitch::OnSwitch(QWidget* parent) : QAbstractButton(parent) {
  assert(parent != nullptr);
  setCheckable(true);
  setFocusPolicy(Qt::NoFocus);
  setCursor(Qt::PointingHandCursor);
  setAttribute(Qt::WA_Hover);
  assert(isCheckable());
}

QSize OnSwitch::sizeHint() const {
  assert(kSquare > 2 * kInset);
  assert(kRoom >= 0);
  return QSize(kSquare + 2 * kRoom, kSquare + 2 * kRoom);
}

void OnSwitch::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(kSquare > 2 * kInset);
  QPainter painter(this);
  const QRect square((width() - kSquare) / 2, (height() - kSquare) / 2,
                     kSquare, kSquare);
  const bool is_hot = underMouse();
  painter.fillRect(square, QColor(0x18, 0x18, 0x1a));
  painter.setPen(is_hot ? theme::kPick : QColor(0x60, 0x60, 0x66));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(square.adjusted(0, 0, -1, -1));
  const bool is_on = isChecked();
  if (is_on) {
    painter.fillRect(square.adjusted(kInset, kInset, -kInset, -kInset),
                     theme::kPick);
  }
}

}  // namespace snapper
