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

QPolygonF Diamond(QPointF center) {
  assert(std::isfinite(center.x()));
  assert(kKeySize > 0.0);
  return QPolygonF({center + QPointF(0, -kKeySize),
                    center + QPointF(kKeySize, 0),
                    center + QPointF(0, kKeySize),
                    center + QPointF(-kKeySize, 0)});
}

}  // namespace

void TimelineView::PaintRows(QPainter* painter,
                             const std::vector<TimelineRow>& rows) {
  assert(painter != nullptr);
  assert(!rows.empty());
  const Project& project = managers_.history->current();
  const auto& picked = managers_.selection->keys();
  painter->setRenderHint(QPainter::Antialiasing);
  double top = kRulerHeight + kWaveHeight;
  for (const TimelineRow& row : rows) {
    const bool is_current =
        row.layer.IsValid() && row.layer == managers_.selection->layer();
    const QRectF name(0, top, kNameWidth, kRowHeight);
    painter->fillRect(name, is_current ? theme::kPick.darker(160)
                                       : theme::kFace);
    painter->setPen(theme::kText);
    painter->drawText(name.adjusted(6, 0, -4, 0),
                      Qt::AlignVCenter | Qt::AlignLeft, row.name);
    painter->setPen(theme::kFaceDark);
    painter->drawLine(QPointF(0, top + kRowHeight),
                      QPointF(width(), top + kRowHeight));
    const double middle = top + kRowHeight / 2.0;
    for (size_t i = 0; i < row.keys.size(); ++i) {
      const double x = XOf(row.keys[i]) + frame_width_ / 2.0;
      const bool has_next = i + 1 < row.keys.size();
      const double next = has_next ? XOf(row.keys[i + 1]) + frame_width_ / 2.0
                                   : x + frame_width_;
      // A hold is a bar; an ease or slide is a thin line.
      const bool is_eased = row.is_eased[i] && has_next;
      painter->fillRect(QRectF(x, middle - (is_eased ? 1.0 : 4.0), next - x,
                               is_eased ? 2.0 : 8.0),
                        theme::kFaceLight);
      const bool is_picked = std::any_of(
          picked.begin(), picked.end(), [&](const KeyRef& key) {
            return key.frame == row.keys[i] &&
                   std::find(row.tracks.begin(), row.tracks.end(),
                             key.track) != row.tracks.end();
          });
      painter->setPen(QPen(theme::kShadow, 1.0));
      painter->setBrush(is_picked ? theme::kPick : theme::kText);
      painter->drawPolygon(Diamond(QPointF(x, middle)));
    }
    top += kRowHeight;
  }
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
}

}  // namespace snapper
