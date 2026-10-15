#include "ui/brush_well.h"

#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cassert>

#include "edit/draw_manager.h"
#include "edit/palette_manager.h"
#include "ui/deck_paint.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kWide = 56;
constexpr int kTall = 64;
// The dot never outgrows the display; bigger brushes show at its cap.
constexpr double kMaxDot = 18.0;

}  // namespace

BrushWell::BrushWell(const Managers& managers) : managers_(managers) {
  assert(managers_.draw != nullptr);
  assert(managers_.palette != nullptr);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  setToolTip(tr("The brush: its size ([ and ]) and color."));
  const auto repaint = [this] { update(); };
  connect(managers_.draw, &DrawManager::Changed, this, repaint);
  connect(managers_.palette, &PaletteManager::Changed, this, repaint);
}

QSize BrushWell::sizeHint() const { return QSize(kWide, kTall); }

void BrushWell::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(managers_.draw != nullptr);
  QPainter painter(this);
  const QRect glass = rect().adjusted(1, 1, -1, -1);
  deck::PaintDisplay(&painter, glass);
  const int size = managers_.draw->brush_size();
  const double dot = std::clamp(static_cast<double>(size), 1.0, kMaxDot);
  const QPointF middle(glass.center().x() + 0.5, glass.top() + 22.0);
  // A square dot: the brush is square pixels.
  const QRectF tip(middle.x() - dot / 2.0, middle.y() - dot / 2.0, dot, dot);
  painter.fillRect(tip, managers_.palette->current());
  painter.setPen(QPen(deck::kGlowDim, 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(tip.adjusted(-1, -1, 0, 0));
  deck::PaintGlowText(&painter,
                      QRectF(glass.left() + 4, glass.bottom() - 20,
                             glass.width() - 8, 16),
                      tr("%1 px").arg(size), true);
}

}  // namespace snapper
