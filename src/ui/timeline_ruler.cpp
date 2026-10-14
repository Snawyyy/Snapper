#include "ui/timeline_ruler.h"

#include <QString>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

#include "base/frame.h"
#include "ui/paint_clip.h"
#include "ui/theme.h"
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

// One tick at x for frame, and its number when has_label.
void PaintMark(QPainter* painter, double x, int frame, bool has_label,
               double label_x) {
  assert(painter != nullptr);
  assert(frame >= 0);
  const bool is_second = frame % kFramesPerSecond == 0;
  const bool is_two = frame % 2 == 0;
  painter->setPen(is_second ? theme::kText : theme::kTextOff);
  const double tick = is_second ? kRulerHeight : (is_two ? 6.0 : 3.0);
  painter->drawLine(QPointF(x, kRulerHeight - tick),
                    QPointF(x, kRulerHeight));
  if (has_label) {
    painter->drawText(QRectF(label_x - kMinLabelGap / 2.0, 0, kMinLabelGap,
                             kRulerHeight - 4),
                      Qt::AlignHCenter | Qt::AlignVCenter,
                      QString::number(frame));
  }
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
  painter->fillRect(QRectF(0, 0, width, kRulerHeight), theme::kFace);
  const double left = scale.left();
  const bool has_room = width > left;
  if (!has_room) {
    return;
  }
  // Past the last frame is dimmed: nothing plays there.
  const double end = std::max(scale.XOf(scale.length()), left);
  painter->fillRect(QRectF(end, 0, std::max(0.0, width - end), kRulerHeight),
                    theme::kFaceDark);
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
  painter->setPen(QPen(theme::kPick, 2.0));
  painter->drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
  // The frame the playhead is on, in a tag on the ruler.
  const QString number = QString::number(frame);
  const double tag_width =
      painter->fontMetrics().horizontalAdvance(number) + 8.0;
  const QRectF tag(x - tag_width / 2.0, area.top(), tag_width, kRulerHeight);
  painter->fillRect(tag, theme::kPick);
  painter->setPen(theme::kText);
  painter->drawText(tag, Qt::AlignCenter, number);
}

}  // namespace snapper
