// TimelineView's rows: names, keys, holds and eases.

#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/theme.h"
#include "ui/timeline_layout.h"
#include "ui/timeline_view.h"

namespace snapper {
namespace {

QPolygonF Diamond(QPointF center, double grow) {
  assert(std::isfinite(center.x()));
  assert(kKeySize > 0.0 && grow >= 1.0);
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
  double top = kRulerHeight + kWaveHeight;
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
    for (size_t i = 0; i < row.keys.size(); ++i) {
      const bool is_shown = !is_ghost || IsKeyPicked(row, row.keys[i]);
      if (is_shown) {
        PaintKey(painter, row, i, row_index, top + kRowHeight / 2.0);
      }
    }
    top += kRowHeight;
    ++row_index;
  }
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

void TimelineView::PaintKey(QPainter* painter, const TimelineRow& row,
                            size_t i, size_t row_index,
                            double middle) const {
  assert(painter != nullptr);
  assert(i < row.keys.size());
  const double x = XOf(row.keys[i]) + frame_width_ / 2.0;
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
  const double grow = hover_.ScaleOf(GrabKind::kKey, key_name);
  // A hovered key also brightens.
  const QColor fill = is_picked ? theme::kPick : theme::kText;
  painter->setBrush(grow > 1.0 ? fill.lighter(125) : fill);
  painter->drawPolygon(Diamond(QPointF(x, middle), grow));
}

}  // namespace snapper
