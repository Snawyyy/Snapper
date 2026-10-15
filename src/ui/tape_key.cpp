#include "ui/tape_key.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cassert>

#include "ui/snapper_style.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The front edge's share of the height when the key is up.
constexpr double kLipShare = 0.24;
// A latched key rests this far down, so it reads as held in.
constexpr double kLatchDepth = 0.7;
constexpr double kWideKey = 1.9;
constexpr double kNarrowKey = 1.35;
constexpr double kGlyphShare = 0.42;
constexpr int kBrushPitch = 2;
constexpr int kMaxBrushLines = 256;
constexpr QColor kSilverTop{0xdc, 0xdc, 0xe0};
constexpr QColor kSilverBottom{0x9c, 0x9c, 0xa2};
constexpr QColor kLipTop{0x86, 0x86, 0x8c};
constexpr QColor kLipBottom{0x4c, 0x4c, 0x52};
constexpr QColor kEngrave{0x2c, 0x2c, 0x30};

// A triangle size tall pointing right (or left), its back at x.
void AddTriangle(QPainterPath* path, double x, double mid_y, double size,
                 bool is_right) {
  assert(path != nullptr);
  assert(size > 0.0);
  const double reach = size * 0.85;
  const double tip = is_right ? x + reach : x - reach;
  QPolygonF shape;
  shape << QPointF(x, mid_y - size / 2.0) << QPointF(tip, mid_y)
        << QPointF(x, mid_y + size / 2.0);
  path->addPolygon(shape);
  path->closeSubpath();
}

QPainterPath GlyphPath(TapeGlyph glyph, const QPointF& middle,
                       double size) {
  assert(size > 0.0);
  assert(middle.x() >= 0.0);
  QPainterPath path;
  const double reach = size * 0.85;
  const double bar = size * 0.2;
  const double y = middle.y();
  switch (glyph) {
    case TapeGlyph::kFirst: {
      const double left = middle.x() - (bar + reach) / 2.0;
      path.addRect(QRectF(left, y - size / 2.0, bar, size));
      AddTriangle(&path, left + bar + reach, y, size, false);
      break;
    }
    case TapeGlyph::kBack:
      AddTriangle(&path, middle.x() + reach / 2.0, y, size, false);
      break;
    case TapeGlyph::kPlay:
    case TapeGlyph::kNext:
      AddTriangle(&path, middle.x() - reach / 2.0, y, size, true);
      break;
    case TapeGlyph::kLast: {
      const double left = middle.x() - (bar + reach) / 2.0;
      AddTriangle(&path, left, y, size, true);
      path.addRect(QRectF(left + reach, y - size / 2.0, bar, size));
      break;
    }
  }
  return path;
}

// Faint light and dark streaks across the top, the brushed metal grain.
void PaintBrush(QPainter* painter, const QRectF& top) {
  assert(painter != nullptr);
  assert(kBrushPitch > 0);
  const QColor light(255, 255, 255, 22);
  const QColor dark(0, 0, 0, 16);
  for (int line = 0; line < kMaxBrushLines; ++line) {
    const double y = top.top() + 1.0 + line * kBrushPitch;
    const bool is_past = y >= top.bottom() - 1.0;
    if (is_past) {
      break;
    }
    painter->setPen(line % 2 == 0 ? light : dark);
    painter->drawLine(QPointF(top.left() + 1.0, y),
                      QPointF(top.right() - 1.0, y));
  }
}

}  // namespace

TapeKey::TapeKey(TapeGlyph glyph) : glyph_(glyph) {
  assert(kLipShare > 0.0 && kLipShare < 0.5);
  assert(kLatchDepth > 0.0 && kLatchDepth <= 1.0);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  setCursor(Qt::PointingHandCursor);
}

QSize TapeKey::sizeHint() const {
  const int height = QPushButton::sizeHint().height();
  assert(height > 0);
  assert(kWideKey > kNarrowKey);
  const bool is_wide = glyph_ == TapeGlyph::kPlay;
  const double share = is_wide ? kWideKey : kNarrowKey;
  return {static_cast<int>(height * share), height};
}

double TapeKey::Depth() const {
  assert(kLatchDepth > 0.0);
  const double press = property(kPressProperty).toDouble();
  assert(press == press);
  const double latch = isChecked() ? kLatchDepth : 0.0;
  // The press spring may dip under 0: the key bounces a hair high.
  return std::clamp(std::max(press, latch), -0.2, 1.0);
}

void TapeKey::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(height() > 0);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);
  // The slot the key sits in, seen above it as it sinks.
  painter.fillRect(rect(), theme::kShadow);
  const bool is_greyed = !isEnabled();
  if (is_greyed) {
    painter.setOpacity(0.45);
  }
  const double lip = height() * kLipShare;
  const double drop = Depth() * lip * 0.8;
  const QRectF top(1.0, 1.0 + drop, width() - 2.0, height() - 2.0 - lip);
  const QRectF front(top.left(), top.bottom(), top.width(),
                     height() - 1.0 - top.bottom());
  const double hover = property(kHoverProperty).toDouble();
  const int lift = 100 + static_cast<int>(12.0 * hover);
  QLinearGradient metal(top.topLeft(), top.bottomLeft());
  metal.setColorAt(0.0, kSilverTop.lighter(lift));
  metal.setColorAt(1.0, kSilverBottom.lighter(lift));
  painter.fillRect(top, metal);
  PaintBrush(&painter, top);
  QLinearGradient edge(front.topLeft(), front.bottomLeft());
  edge.setColorAt(0.0, kLipTop);
  edge.setColorAt(1.0, kLipBottom);
  painter.fillRect(front, edge);
  painter.setPen(QColor(255, 255, 255, 170));
  painter.drawLine(top.topLeft(), top.topRight());
  painter.setPen(theme::kShadow);
  painter.drawRect(top.united(front).adjusted(0, 0, -1, -1));
  PaintGlyph(&painter, top);
}

void TapeKey::PaintGlyph(QPainter* painter, const QRectF& top) const {
  assert(painter != nullptr);
  assert(top.height() > 0.0);
  const bool is_wide = glyph_ == TapeGlyph::kPlay;
  const double size = top.height() * kGlyphShare * (is_wide ? 1.15 : 1.0);
  const QPainterPath glyph = GlyphPath(glyph_, top.center(), size);
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(Qt::NoPen);
  // A light rim under the symbol makes it look pressed into the metal.
  painter->fillPath(glyph.translated(0.0, 1.0), QColor(255, 255, 255, 140));
  // A latched key wears red: what is on is red here.
  const bool is_latched = isChecked();
  painter->fillPath(glyph, is_latched ? theme::kPick : kEngrave);
}

}  // namespace snapper
