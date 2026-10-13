#include "ui/snapper_accents.h"

#include <algorithm>
#include <cassert>

#include "ui/theme.h"

namespace snapper {

void DrawAccentStrip(QPainter* painter, const QRect& strip) {
  assert(painter != nullptr);
  assert(theme::kPick.isValid());
  const bool has_room = strip.isValid();
  if (!has_room) {
    return;
  }
  painter->fillRect(strip, theme::kPick);
}

void DrawFocusRing(QPainter* painter, const QRect& box) {
  assert(painter != nullptr);
  assert(theme::kPick.isValid());
  const bool has_room = box.width() > 2 && box.height() > 2;
  if (!has_room) {
    return;
  }
  painter->save();
  painter->setPen(theme::kPick);
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(box.adjusted(0, 0, -1, -1));
  painter->restore();
}

void DrawCheckIndicator(QPainter* painter, const QStyleOption& option,
                        bool is_radio) {
  assert(painter != nullptr);
  assert(theme::kField.isValid());
  const QStyle::State state = option.state;
  const bool is_enabled = state.testFlag(QStyle::State_Enabled);
  const QRect box = option.rect.adjusted(0, 0, -1, -1);
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, is_radio);
  painter->setPen(is_enabled ? theme::kShadow : theme::kFaceDark);
  painter->setBrush(is_enabled ? theme::kField : theme::kFace);
  if (is_radio) {
    painter->drawEllipse(box);
  } else {
    painter->drawRect(box);
  }
  const bool is_on = state.testFlag(QStyle::State_On);
  const bool is_half = state.testFlag(QStyle::State_NoChange);
  const QColor mark = is_enabled ? theme::kPick : theme::kTextOff;
  const int pad = std::max(2, box.width() / 4);
  painter->setPen(Qt::NoPen);
  painter->setBrush(mark);
  if (is_on) {
    const QRect inner = box.adjusted(pad, pad, -pad, -pad);
    if (is_radio) {
      painter->drawEllipse(inner);
    } else {
      painter->drawRect(inner);
    }
  } else if (is_half) {
    const QRect dash(box.left() + pad, box.center().y() - 1,
                     box.width() - 2 * pad + 1, 2);
    painter->drawRect(dash);
  }
  painter->restore();
}

}  // namespace snapper
