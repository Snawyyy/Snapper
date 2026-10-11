// ShotStrip's drawing: the chips with their first frames, and the + block.

#include <QPainter>
#include <QPixmap>

#include <algorithm>
#include <cassert>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/shot_strip.h"
#include "ui/shot_strip_layout.h"
#include "ui/theme.h"

namespace snapper {
std::vector<ShotStrip::Block> ShotStrip::Blocks() const {
  assert(managers_.history != nullptr);
  std::vector<Block> blocks;
  double x = 2.0;
  for (const auto& shot : managers_.history->current().shots) {
    const double width = WidthOf(*shot);
    blocks.push_back({shot->id, QRectF(x, 2.0, width, kStripHeight - 4.0)});
    x += width + 2.0;
  }
  assert(blocks.size() <= static_cast<size_t>(kMaxShots));
  return blocks;
}

QRectF ShotStrip::AddBlock() const {
  assert(managers_.history != nullptr);
  const auto blocks = Blocks();
  const double x = blocks.empty() ? 2.0 : blocks.back().rect.right() + 2.0;
  assert(x >= 0.0);
  return QRectF(x, 2.0, kAddWidth, kStripHeight - 4.0);
}

// The first frame at the left of a chip long enough to show it; where
// the words may start.
double ShotStrip::PaintPicture(QPainter* painter, const QRectF& card,
                               ShotId id) const {
  assert(painter != nullptr);
  assert(card.isValid());
  const bool has_picture = card.width() >= kPictureFrom;
  if (!has_picture) {
    return card.left() + 8.0;
  }
  const Project& project = managers_.history->current();
  const auto& shared = *std::find_if(
      project.shots.begin(), project.shots.end(),
      [id](const auto& s) { return s->id == id; });
  const QPixmap frame = thumbs_.Of(project, shared, kStripThumbWidth,
                                   managers_.history->IsScopeOpen());
  const QRectF slot(card.left() + 4.0, card.top() + 4.0, kPictureWidth,
                    card.height() - 12.0);
  painter->fillRect(slot, theme::kField);
  painter->drawPixmap(slot, frame, QRectF(frame.rect()));
  return slot.right() + 6.0;
}

void ShotStrip::PaintBlock(QPainter* painter, const Block& block) const {
  assert(painter != nullptr);
  assert(block.rect.isValid());
  const Project& project = managers_.history->current();
  const Shot& shot = *FindShot(project, block.shot);
  const bool is_focus = block.shot == managers_.selection->shot();
  const bool is_picked = managers_.selection->shots().contains(block.shot);
  const bool is_hover = block.shot == hovered_;
  // A hovered chip lifts a pixel and brightens.
  const QRectF card = block.rect.translated(0.0, is_hover ? -1.0 : 0.0);
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setPen(Qt::NoPen);
  painter->setBrush(is_picked || is_hover ? theme::kFaceLight : theme::kFace);
  painter->drawRoundedRect(card, 5.0, 5.0);
  // The shot's colour as a bar along the bottom.
  painter->setBrush(shot.background);
  painter->drawRoundedRect(
      QRectF(card.left() + 5.0, card.bottom() - 5.0, card.width() - 10.0,
             3.0),
      1.5, 1.5);
  const double text_left = PaintPicture(painter, card, block.shot);
  painter->setPen(QPen(is_picked ? theme::kPick : theme::kShadow,
                       is_focus ? 2.0 : 1.0));
  painter->setBrush(Qt::NoBrush);
  painter->drawRoundedRect(card, 5.0, 5.0);
  const double edge = HoverAmount(hover_.ScaleOf(
      GrabKind::kEdge, QString::number(block.shot.value())));
  const bool shows_edge = edge > 0.0;
  if (shows_edge) {
    painter->fillRect(
        QRectF(card.right() - 3 - 3 * edge, card.top() + 3.0, 3 + 3 * edge,
               card.height() - 6.0),
        theme::kHandle.lighter(100 + int(40 * edge)));
  }
  const QRectF words(text_left, card.top() + 3.0,
                     card.right() - text_left - 6.0, card.height() - 10.0);
  QFont bold = painter->font();
  bold.setBold(true);
  painter->setFont(bold);
  painter->setPen(theme::kText);
  painter->drawText(words, Qt::AlignLeft | Qt::AlignTop,
                    painter->fontMetrics().elidedText(
                        shot.name, Qt::ElideRight,
                        static_cast<int>(words.width())));
  bold.setBold(false);
  painter->setFont(bold);
  painter->setPen(theme::kTextOff);
  painter->drawText(words, Qt::AlignLeft | Qt::AlignBottom,
                    tr("%1 s").arg(shot.length.index() / 24.0, 0, 'f', 1));
}

void ShotStrip::paintEvent(QPaintEvent* event) {
  assert(managers_.history != nullptr);
  assert(event != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), theme::kFaceDark);
  for (const Block& block : Blocks()) {
    PaintBlock(&painter, block);
  }
  const QRectF add = AddBlock();
  painter.fillRect(add, theme::kFace);
  painter.setPen(theme::kText);
  painter.drawText(add, Qt::AlignCenter, QStringLiteral("+"));
  feedback_.Paint(&painter, QRectF(rect()));
}

}  // namespace snapper
