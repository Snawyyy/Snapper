#include <QKeyEvent>
#include <QPainter>

#include <cassert>

#include "anim/light_table.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "ui/stage_view.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// Earlier frames show through in Teto red, later ones in teal.
constexpr QColor kBefore{0xd8, 0x30, 0x4f};
constexpr QColor kAfter{0x2e, 0x9c, 0x88};

// frame with its dark lines turned tint and its white left white: Screen
// lifts black to the tint and leaves white be.
QImage Tinted(QImage frame, const QColor& tint) {
  assert(!frame.isNull());
  assert(tint.isValid());
  QPainter painter(&frame);
  painter.setCompositionMode(QPainter::CompositionMode_Screen);
  painter.fillRect(frame.rect(), tint);
  return frame;
}

}  // namespace

void StageView::SetLightTable(bool is_on) {
  if (is_on == is_light_table_on_) {
    return;
  }
  is_light_table_on_ = is_on;
  emit LightTableChanged(is_on);
  update();
}

bool StageView::LightTableKey(QKeyEvent* event) {
  assert(event != nullptr);
  const bool is_light_key = event->key() == Qt::Key_L &&
                            event->modifiers() == Qt::NoModifier &&
                            !event->isAutoRepeat();
  if (!is_light_key) {
    return false;
  }
  SetLightTable(!is_light_table_on_);
  feedback_.Pulse(QPointF(width() / 2.0, height() / 2.0),
                  is_light_table_on_ ? tr("Light table on")
                                     : tr("Light table off"));
  return true;
}

void StageView::PaintLightTable(const StageFrame& frame, QPainter* painter) {
  assert(painter != nullptr);
  assert(frame.scale > 0.0);
  if (!is_light_table_on_) {
    return;
  }
  const Project& project = managers_.history->current();
  const Frame now = managers_.playback->FrameOn(Timeline::kShots);
  painter->save();
  // Multiply: the paper stays as it is, the lines show through it.
  painter->setCompositionMode(QPainter::CompositionMode_Multiply);
  for (const OnionSkin& skin :
       LightTableSkins(project, now, kLightTableReach)) {
    const QImage seen = renderer_.RenderFrame(project, skin.master,
                                              frame.scale);
    painter->setOpacity(skin.opacity);
    painter->drawImage(frame.corner,
                       Tinted(seen, skin.is_before ? kBefore : kAfter));
  }
  painter->restore();
  // The lamp: a small plate saying the table is lit.
  const QString lamp = tr("LIGHT TABLE (L)");
  const QRect plate(width() - painter->fontMetrics().horizontalAdvance(lamp) -
                        26,
                    10, painter->fontMetrics().horizontalAdvance(lamp) + 16,
                    painter->fontMetrics().height() + 6);
  painter->fillRect(plate, theme::kRaised);
  painter->fillRect(QRect(plate.left(), plate.bottom() - 1, plate.width(), 2),
                    theme::kHandle);
  painter->setPen(theme::kHandle);
  painter->drawText(plate, Qt::AlignCenter, lamp);
}

}  // namespace snapper
