// TimelineView's chip bar: where it floats over the picked keys.

#include <QResizeEvent>

#include <algorithm>
#include <cassert>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/timeline_layout.h"
#include "ui/timeline_view.h"

namespace snapper {

void TimelineView::resizeEvent(QResizeEvent* event) {
  assert(event != nullptr);
  assert(width() >= 0);
  QWidget::resizeEvent(event);
  ShowChips();
}

void TimelineView::ShowChips() {
  assert(managers_.selection != nullptr);
  assert(FrameWidth() > 0.0);
  chips_.Refresh();
  const bool has_keys = !managers_.selection->keys().empty();
  const auto rows =
      TimelineRows(managers_.history->current(), managers_.selection->shot());
  const bool has_room = width() > kNameWidth;
  const bool is_hidden = !has_keys || rows.empty() || !has_room;
  if (is_hidden) {
    chips_.hide();
    return;
  }
  // The box round the picked keys that are in view.
  const QRectF area = FrameArea(RowsTop());
  bool is_any = false;
  QRectF box;
  double top = RowsTop();
  for (const TimelineRow& row : rows) {
    for (const Frame frame : row.keys) {
      const double x = XOf(frame) + FrameWidth() / 2.0;
      const bool is_seen = IsKeyPicked(row, frame) && x >= area.left() &&
                           x <= area.right();
      if (is_seen) {
        const QRectF here(x, top, 0.0, kRowHeight);
        box = is_any ? box.united(here) : here;
        is_any = true;
      }
    }
    top += kRowHeight;
  }
  if (!is_any) {
    chips_.hide();
    return;
  }
  chips_.adjustSize();
  chips_.move(KeyChipBar::SpotFor(box, chips_.size(), FrameArea(kRulerHeight)));
  chips_.raise();
}

}  // namespace snapper
