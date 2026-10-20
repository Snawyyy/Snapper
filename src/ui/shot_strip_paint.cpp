// ShotStrip's drawing: the shots as pinned polaroids, and the + block.

#include <QPainter>
#include <QPixmap>

#include <algorithm>
#include <cassert>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/checkerboard.h"
#include "ui/shot_strip.h"
#include "ui/shot_strip_layout.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// A see-through shot's picture: the checker squares' side.
constexpr double kThumbChecker = 4.0;
// A shot is a polaroid pinned to the strip: off-white print, ink words.
constexpr QColor kPrint{0xf4, 0xf1, 0xea};
constexpr QColor kPrintShade{0xd9, 0xd3, 0xc6};
constexpr QColor kInk{0x3a, 0x34, 0x2c};
constexpr QColor kInkQuiet{0x7a, 0x72, 0x66};
// Room above a card for its pin.
constexpr double kPinRoom = 5.0;

}  // namespace

std::vector<ShotStrip::Block> ShotStrip::Blocks() const {
  assert(managers_.history != nullptr);
  std::vector<Block> blocks;
  double x = 2.0 - scroll_;
  for (const auto& shot : managers_.history->current().shots) {
    const double width = WidthOf(*shot);
    blocks.push_back({shot->id, QRectF(x, 2.0 + kPinRoom, width,
                                       kStripHeight - 6.0 - kPinRoom)});
    x += width + 2.0;
  }
  assert(blocks.size() <= static_cast<size_t>(kMaxShots));
  return blocks;
}

QRectF ShotStrip::AddBlock() const {
  assert(managers_.history != nullptr);
  const auto blocks = Blocks();
  const double x =
      blocks.empty() ? 2.0 - scroll_ : blocks.back().rect.right() + 2.0;
  return QRectF(x, 2.0 + kPinRoom, kAddWidth,
                kStripHeight - 6.0 - kPinRoom);
}

// The first frame at the left of a chip; where the words may start.
double ShotStrip::PaintPicture(QPainter* painter, const QRectF& card,
                               ShotId id) const {
  assert(painter != nullptr);
  assert(card.isValid());
  assert(card.width() >= kPictureWidth);
  const Project& project = managers_.history->current();
  const auto& shared = *std::find_if(
      project.shots.begin(), project.shots.end(),
      [id](const auto& s) { return s->id == id; });
  const QPixmap frame = thumbs_.Of(project, shared, kStripThumbWidth,
                                   managers_.history->IsScopeOpen());
  // The print's white border shows round the picture.
  const QRectF slot(card.left() + 4.0, card.top() + 4.0, kPictureWidth,
                    card.height() - 8.0);
  painter->fillRect(slot, theme::kField);
  // The frame keeps its shape and fills the slot, cropped like a print.
  painter->save();
  painter->setClipRect(slot);
  const QSizeF fit = QSizeF(frame.size()).scaled(
      slot.size(), Qt::KeepAspectRatioByExpanding);
  const QRectF picture(
      slot.center() - QPointF(fit.width(), fit.height()) / 2.0, fit);
  const bool is_clear = shared->is_transparent;
  if (is_clear) {
    PaintCheckerboard(painter, picture, kThumbChecker);
  }
  painter->drawPixmap(picture, frame, QRectF(frame.rect()));
  if (is_clear) {
    PaintClearBadge(painter, picture);
  }
  painter->restore();
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
  // A hovered print lifts off the strip.
  const QRectF card = block.rect.translated(0.0, is_hover ? -2.0 : 0.0);
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setPen(Qt::NoPen);
  painter->fillRect(card.translated(1.0, is_hover ? 3.0 : 2.0),
                    QColor(0, 0, 0, 120));
  painter->fillRect(card, is_picked || is_hover ? kPrint : kPrintShade);
  // The shot's colour as a strip of tape along its foot.
  painter->fillRect(QRectF(card.left() + kPictureWidth + 10.0,
                           card.bottom() - 4.0,
                           card.width() - kPictureWidth - 16.0, 3.0),
                    shot.background);
  const double text_left = PaintPicture(painter, card, block.shot);
  painter->setPen(QPen(is_picked ? theme::kPick : kInkQuiet,
                       is_picked && is_focus ? 2.0 : 1.0));
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(card);
  // The pin it hangs on.
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(is_picked ? theme::kPick : theme::kTextOff);
  painter->drawEllipse(QPointF(card.center().x(), card.top() + 1.0), 3.5,
                       3.5);
  const double edge = HoverAmount(hover_.ScaleOf(
      GrabKind::kEdge, QString::number(block.shot.value())));
  const bool shows_edge = edge > 0.0;
  if (shows_edge) {
    painter->fillRect(
        QRectF(card.right() - 3 - 3 * edge, card.top() + 3.0, 3 + 3 * edge,
               card.height() - 6.0),
        theme::kHandle.lighter(100 + int(40 * edge)));
  }
  // The name written on the print, the length under it.
  const QRectF words(text_left, card.top() + 5.0,
                     card.right() - text_left - 8.0, card.height() - 12.0);
  QFont hand = painter->font();
  hand.setBold(true);
  hand.setItalic(true);
  painter->setFont(hand);
  painter->setPen(kInk);
  painter->drawText(words, Qt::AlignLeft | Qt::AlignTop,
                    painter->fontMetrics().elidedText(
                        shot.name, Qt::ElideRight,
                        static_cast<int>(words.width())));
  hand.setBold(false);
  painter->setFont(hand);
  painter->setPen(kInkQuiet);
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
  // A blank print, dashed, waiting to be taken.
  const QRectF add = AddBlock();
  painter.setPen(QPen(theme::kTextOff, 1.0, Qt::DashLine));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(add.adjusted(0.5, 0.5, -0.5, -0.5));
  painter.setPen(theme::kText);
  painter.drawText(add, Qt::AlignCenter, QStringLiteral("+"));
  feedback_.Paint(&painter, QRectF(rect()));
}

}  // namespace snapper
