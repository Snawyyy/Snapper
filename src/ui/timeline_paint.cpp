// TimelineView's rows: names, keys, holds and eases.

#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/change_effects.h"
#include "ui/paint_clip.h"
#include "ui/theme.h"
#include "ui/timeline_layout.h"
#include "ui/timeline_view.h"

namespace snapper {
namespace {

QPolygonF Diamond(QPointF center, double grow) {
  assert(std::isfinite(center.x()));
  assert(kKeySize > 0.0 && grow >= 0.0);
  const double size = kKeySize * grow;
  return QPolygonF({center + QPointF(0, -size), center + QPointF(size, 0),
                    center + QPointF(0, size), center + QPointF(-size, 0)});
}

}  // namespace

void TimelineView::PaintRows(QPainter* painter,
                             const std::vector<TimelineRow>& rows,
                             bool is_ghost) {
  assert(painter != nullptr);
  assert(!rows.empty());
  painter->setRenderHint(QPainter::Antialiasing);
  double top = RowsTop();
  size_t row_index = 0;
  for (const TimelineRow& row : rows) {
    const bool is_current =
        row.layer.IsValid() && row.layer == managers_.selection->layer();
    const QRectF name(0, top, kNameWidth, kRowHeight);
    // A ghost is only the keys: no names, no row lines.
    if (!is_ghost) {
      painter->fillRect(name, is_current ? theme::kPick.darker(160)
                                         : theme::kFace);
      painter->setPen(theme::kText);
      painter->drawText(name.adjusted(6, 0, -4, 0),
                        Qt::AlignVCenter | Qt::AlignLeft, row.name);
      painter->setPen(theme::kFaceDark);
      painter->drawLine(QPointF(0, top + kRowHeight),
                        QPointF(width(), top + kRowHeight));
    }
    const double middle = top + kRowHeight / 2.0;
    // Scrolled keys, bars and puffs never reach over the names.
    const PaintClip keep_in(painter, FrameArea(RowsTop()));
    PaintKeys(painter, row, row_index, middle, is_ghost);
    if (!is_ghost) {
      PaintPuffs(painter, row, middle);
    }
    top += kRowHeight;
    ++row_index;
  }
}

QRectF TimelineView::FrameArea(double top) const {
  assert(width() > kNameWidth);
  assert(top >= 0.0);
  return QRectF(kNameWidth, top, width() - kNameWidth, height() - top);
}

bool TimelineView::IsKeyPicked(const TimelineRow& row, Frame frame) const {
  assert(frame.index() >= 0);
  assert(managers_.selection != nullptr);
  const auto& picked = managers_.selection->keys();
  return std::any_of(picked.begin(), picked.end(), [&](const KeyRef& key) {
    return key.frame == frame &&
           std::find(row.tracks.begin(), row.tracks.end(), key.track) !=
               row.tracks.end();
  });
}

void TimelineView::PaintKeys(QPainter* painter, const TimelineRow& row,
                             size_t row_index, double middle,
                             bool is_ghost) {
  assert(painter != nullptr);
  assert(row.keys.size() == row.is_eased.size());
  for (size_t i = 0; i < row.keys.size(); ++i) {
    const bool is_shown = !is_ghost || IsKeyPicked(row, row.keys[i]);
    if (!is_shown) {
      continue;
    }
    const int lane = row.layer.value();
    const double slide = effects_.SlideOffsetOf(lane, 0, row.keys[i]);
    const double x = XOf(row.keys[i]) + frame_width_ / 2.0 +
                     slide * frame_width_;
    const bool has_next = i + 1 < row.keys.size();
    const double next = has_next ? XOf(row.keys[i + 1]) + frame_width_ / 2.0
                                 : x + frame_width_;
    // A hold is a bar; an ease or slide is a thin line.
    const bool is_eased = row.is_eased[i] && has_next;
    painter->fillRect(QRectF(x, middle - (is_eased ? 1.0 : 4.0), next - x,
                             is_eased ? 2.0 : 8.0),
                      theme::kFaceLight);
    const bool is_picked = IsKeyPicked(row, row.keys[i]);
    painter->setPen(QPen(theme::kShadow, 1.0));
    const QString key_name = KeyName(row_index, row.keys[i]);
    // A hovered key grows and brightens; so does one popping in, and
    // one coming back by undo flashes brighter still.
    const double grow = hover_.ScaleOf(GrabKind::kKey, key_name) *
                        effects_.ScaleOf(lane, 0, row.keys[i]);
    const double glow = effects_.GlowOf(lane, 0, row.keys[i]);
    const QColor fill = is_picked ? theme::kPick : theme::kText;
    const bool is_lit = grow > 1.0;
    painter->setBrush(fill.lighter((is_lit ? 125 : 100) +
                                   static_cast<int>(100.0 * glow)));
    painter->drawPolygon(Diamond(QPointF(x, middle), grow));
    PaintRing(painter, QPointF(x, middle),
              effects_.RingOf(lane, 0, row.keys[i]));
  }
}

void TimelineView::PaintPuffs(QPainter* painter, const TimelineRow& row,
                              double middle) {
  assert(painter != nullptr);
  assert(middle > 0.0);
  for (const ChangeEffect& puff : effects_.Puffs()) {
    const bool is_here = puff.lane == row.layer.value();
    if (!is_here) {
      continue;
    }
    QColor fill = theme::kText;
    fill.setAlphaF(static_cast<float>(PuffOpacity(puff.t)));
    painter->setPen(Qt::NoPen);
    painter->setBrush(fill);
    const double x = XOf(puff.frame) + frame_width_ / 2.0;
    painter->drawPolygon(Diamond(QPointF(x, middle), PuffScale(puff.t)));
  }
}

void TimelineView::PaintRing(QPainter* painter, QPointF center,
                             std::optional<double> progress) {
  assert(painter != nullptr);
  assert(std::isfinite(center.x()) && std::isfinite(center.y()));
  const bool is_ringed = progress.has_value();
  if (!is_ringed) {
    return;
  }
  QColor ring = theme::kPick;
  ring.setAlphaF(static_cast<float>(1.0 - *progress));
  painter->setPen(QPen(ring, 2.0));
  painter->setBrush(Qt::NoBrush);
  const double radius = kKeySize * (1.0 + 1.6 * *progress);
  painter->drawEllipse(center, radius, radius);
}

}  // namespace snapper
