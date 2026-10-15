#include "ui/tape_spine.h"

#include <QPainter>

#include <cassert>

#include "ui/deck_paint.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kSpineHeight = 24;
// How far a pulled tape stands out of the rack.
constexpr double kPull = 10.0;
constexpr double kBand = 6.0;

}  // namespace

TapeSpine::TapeSpine(const QString& name) {
  assert(!name.isEmpty());
  assert(kSpineHeight > 0);
  setText(name);
  setCheckable(true);
  setCursor(Qt::PointingHandCursor);
  // Repaints as the pointer comes and goes: a hovered tape slides out a
  // little.
  setAttribute(Qt::WA_Hover);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize TapeSpine::sizeHint() const {
  return QSize(fontMetrics().horizontalAdvance(text()) + 60, kSpineHeight);
}

void TapeSpine::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(width() > 0);
  QPainter painter(this);
  const bool is_out = isChecked();
  const bool is_hovered = underMouse() && isEnabled();
  // Pushed in, a tape sits back in the rack's shadow; pulled, it stands
  // its full length.
  const double back = is_out ? 0.0 : (is_hovered ? kPull / 2.0 : kPull);
  const QRectF shell(0.0, 1.0, width() - back, height() - 2.0);
  painter.fillRect(QRectF(shell.right(), 0, back, height()), theme::kShadow);
  painter.fillRect(shell, QColor(0x18, 0x15, 0x14));
  painter.setPen(deck::kPlasticLight);
  painter.drawLine(shell.topLeft(), shell.topRight());
  const QRectF label = shell.adjusted(kBand + 3.0, 3.0, -4.0, -3.0);
  painter.fillRect(label, is_out ? deck::kLabel : deck::kLabelShut);
  painter.fillRect(QRectF(shell.left() + 2.0, shell.top() + 2.0, kBand,
                          shell.height() - 4.0),
                   is_out ? theme::kPick : theme::kPick.darker(220));
  painter.setPen(deck::kInk);
  painter.drawText(label.adjusted(6, 0, -4, 0),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   fontMetrics().elidedText(
                       text(), Qt::ElideRight,
                       static_cast<int>(label.width()) - 10));
}

}  // namespace snapper
