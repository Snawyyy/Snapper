#include "ui/deck_panel.h"

#include <QPainter>

#include <cassert>

#include "ui/deck_paint.h"

namespace snapper {

void DeckPanel::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(width() >= 0);
  QPainter painter(this);
  const bool has_room = height() > 2;
  if (has_room) {
    deck::PaintPanel(&painter, QRectF(rect()));
  }
}

}  // namespace snapper
