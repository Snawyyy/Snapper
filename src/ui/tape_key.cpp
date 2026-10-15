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

// The front face's share of the height when the key is up.
constexpr double kLipShare = 0.26;
// A latched key rests this far down, so it reads as held in.
constexpr double kLatchDepth = 0.7;
constexpr double kKeyShare = 1.1;
// The round dimple's share of the top's short side.
constexpr double kDimpleShare = 0.82;
constexpr double kGlyphShare = 0.42;
constexpr QColor kTopLight{0x44, 0x44, 0x48};
constexpr QColor kTopDark{0x26, 0x26, 0x29};
constexpr QColor kFrontLight{0x16, 0x16, 0x18};
constexpr QColor kFrontDark{0x08, 0x08, 0x09};
constexpr QColor kRidge{0x5e, 0x5e, 0x64};
constexpr QColor kPrint{0xc8, 0xc8, 0xcc};

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

// The round cup pressed into the key top: lit from above, so its upper
// wall is in shadow and its lower rim catches the light.
void PaintDimple(QPainter* painter, const QRectF& cup) {
  assert(painter != nullptr);
  assert(cup.width() > 0.0);
  QLinearGradient bowl(cup.topLeft(), cup.bottomLeft());
  bowl.setColorAt(0.0, QColor(0x08, 0x08, 0x0a));
  bowl.setColorAt(1.0, QColor(0x30, 0x30, 0x34));
  painter->setPen(Qt::NoPen);
  painter->setBrush(bowl);
  painter->drawEllipse(cup);
  QLinearGradient rim(cup.topLeft(), cup.bottomLeft());
  rim.setColorAt(0.0, QColor(0, 0, 0, 160));
  rim.setColorAt(1.0, QColor(255, 255, 255, 110));
  painter->setPen(QPen(QBrush(rim), 1.0));
  painter->setBrush(Qt::NoBrush);
  painter->drawEllipse(cup);
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
  assert(kKeyShare > 0.0);
  return {static_cast<int>(height * kKeyShare), height};
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
  const int lift = 100 + static_cast<int>(25.0 * hover);
  QLinearGradient plastic(top.topLeft(), top.bottomLeft());
  plastic.setColorAt(0.0, kTopLight.lighter(lift));
  plastic.setColorAt(1.0, kTopDark.lighter(lift));
  painter.fillRect(top, plastic);
  QLinearGradient face(front.topLeft(), front.bottomLeft());
  face.setColorAt(0.0, kFrontLight);
  face.setColorAt(1.0, kFrontDark);
  painter.fillRect(front, face);
  // The rounded edge where the top turns down into the front.
  painter.setPen(kRidge);
  painter.drawLine(top.bottomLeft(), top.bottomRight());
  painter.setPen(kRidge.lighter(130));
  painter.drawLine(top.topLeft(), top.topRight());
  painter.setRenderHint(QPainter::Antialiasing, true);
  const double side =
      std::min(top.width(), top.height()) * kDimpleShare;
  QRectF cup(0.0, 0.0, side, side);
  cup.moveCenter(top.center());
  PaintDimple(&painter, cup);
  PaintGlyph(&painter, cup);
}

void TapeKey::PaintGlyph(QPainter* painter, const QRectF& cup) const {
  assert(painter != nullptr);
  assert(cup.height() > 0.0);
  const double size = cup.height() * kGlyphShare;
  const QPainterPath glyph = GlyphPath(glyph_, cup.center(), size);
  painter->setPen(Qt::NoPen);
  // A latched key wears red: what is on is red here.
  const bool is_latched = isChecked();
  painter->fillPath(glyph, is_latched ? theme::kPick : kPrint);
}

}  // namespace snapper
