#include "ui/timeline_ruler.h"

#include <QLinearGradient>
#include <QString>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

#include "base/frame.h"
#include "ui/deck_paint.h"
#include "ui/paint_clip.h"
#include "ui/timeline_layout.h"

namespace snapper {
namespace {

// Frames between marks: single frames, then 2s, 3s, 6s, 12s, then whole
// seconds of 1, 2, 4, 10, 20, 50, 100, 300, 600 and 1800.
constexpr std::array<int, 15> kSteps = {1,   2,    3,    6,    12,
                                        24,  48,   96,   240,  480,
                                        1200, 2400, 7200, 14400, 43200};
// Marks painted at most, whatever the zoom.
constexpr int kMaxMarks = 6000;
// The ruler is an old radio's tuning dial: dark green glass, a scale
// printed in pale green and an orange needle for the playhead.
constexpr QColor kGlassTop{0x1a, 0x40, 0x34};
constexpr QColor kGlassBottom{0x0c, 0x26, 0x1e};
constexpr QColor kGlassPast{0x05, 0x10, 0x0d};
constexpr QColor kPrintBright{0xd8, 0xea, 0xd2};
constexpr QColor kPrintDim{0x5c, 0xa8, 0x82};
constexpr QColor kScaleLine{0x3e, 0x8e, 0x68};
constexpr QColor kNeedle{0xf2, 0x8c, 0x28};
constexpr QColor kNeedleInk{0x1a, 0x10, 0x06};

// One tick at x for frame, and its number when has_label.
void PaintMark(QPainter* painter, double x, int frame, bool has_label,
               double label_x) {
  assert(painter != nullptr);
  assert(frame >= 0);
  const bool is_second = frame % kFramesPerSecond == 0;
  const bool is_two = frame % 2 == 0;
  painter->setPen(is_second ? kPrintBright : kPrintDim);
  // Ticks stand short under the numbers, as printed on a dial, so no
  // line runs through a number.
  const double tick = is_second ? 6.0 : (is_two ? 4.0 : 2.0);
  painter->drawLine(QPointF(x, kRulerHeight - tick),
                    QPointF(x, kRulerHeight));
  if (has_label) {
    painter->drawText(QRectF(label_x - kMinLabelGap / 2.0, 0, kMinLabelGap,
                             kRulerHeight - 5),
                      Qt::AlignHCenter | Qt::AlignVCenter,
                      QString::number(frame));
  }
}

// The green glass from glass.left(), darker past end where nothing
// plays, with a faint sheen and the printed line the ticks stand on.
void PaintDialGlass(QPainter* painter, const QRectF& glass, double end) {
  assert(painter != nullptr);
  assert(end >= glass.left());
  QLinearGradient green(glass.topLeft(), glass.bottomLeft());
  green.setColorAt(0.0, kGlassTop);
  green.setColorAt(1.0, kGlassBottom);
  painter->fillRect(glass, green);
  const double past = std::max(0.0, glass.right() - end);
  painter->fillRect(QRectF(end, glass.top(), past, glass.height()),
                    kGlassPast);
  // Light off the glass: brightest along its top edge.
  QLinearGradient sheen(glass.topLeft(), glass.bottomLeft());
  sheen.setColorAt(0.0, QColor(255, 255, 255, 34));
  sheen.setColorAt(0.45, QColor(255, 255, 255, 0));
  painter->fillRect(glass, sheen);
  painter->setPen(kScaleLine);
  const double base = glass.bottom() - 1.0;
  painter->drawLine(QPointF(glass.left(), base), QPointF(end, base));
}

}  // namespace

int FrameStepFor(double pixels_per_frame, double min_gap) {
  assert(pixels_per_frame > 0.0);
  assert(min_gap > 0.0);
  for (const int step : kSteps) {
    const bool is_roomy = step * pixels_per_frame >= min_gap;
    if (is_roomy) {
      return step;
    }
  }
  return kSteps.back();
}

void PaintFrameRuler(QPainter* painter, const TimelineScale& scale,
                     int width, bool labels_on_middle) {
  assert(painter != nullptr);
  assert(width >= 0);
  painter->fillRect(QRectF(0, 0, width, kRulerHeight), deck::kPlastic);
  const double left = scale.left();
  const bool has_room = width > left;
  if (!has_room) {
    return;
  }
  const double end = std::max(scale.XOf(scale.length()), left);
  PaintDialGlass(painter, QRectF(left, 0, width - left, kRulerHeight), end);
  const PaintClip keep_in(painter, QRectF(left, 0, width - left,
                                          kRulerHeight));
  const double per_frame = scale.pixels_per_frame();
  const int tick_step = FrameStepFor(per_frame, kMinTickGap);
  const int label_step = FrameStepFor(per_frame, kMinLabelGap);
  const int first = std::max(0, scale.FrameAt(left)) / tick_step * tick_step;
  const int last = scale.FrameAt(width);
  const double shift = labels_on_middle ? per_frame / 2.0 : 0.0;
  for (int i = 0; i < kMaxMarks; ++i) {
    const int frame = first + i * tick_step;
    const bool is_past_view = frame > last;
    if (is_past_view) {
      break;
    }
    const double x = scale.XOf(frame);
    PaintMark(painter, x, frame, frame % label_step == 0, x + shift);
  }
}

void PaintTimelinePlayhead(QPainter* painter, double x, int frame,
                           const QRectF& area) {
  assert(painter != nullptr);
  assert(frame >= 0);
  const bool has_area = !area.isEmpty();
  if (!has_area) {
    return;
  }
  const PaintClip keep_in(painter, area);
  // The dial's needle: a soft glow behind a thin orange line.
  QColor glow = kNeedle;
  glow.setAlpha(70);
  painter->setPen(QPen(glow, 4.0));
  painter->drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
  painter->setPen(QPen(kNeedle, 2.0));
  painter->drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
  // The frame the playhead is on, in a tag on the ruler.
  const QString number = QString::number(frame);
  const double tag_width =
      painter->fontMetrics().horizontalAdvance(number) + 8.0;
  const QRectF tag(x - tag_width / 2.0, area.top(), tag_width, kRulerHeight);
  painter->fillRect(tag, kNeedle);
  painter->setPen(kNeedleInk);
  painter->drawText(tag, Qt::AlignCenter, number);
}

}  // namespace snapper
