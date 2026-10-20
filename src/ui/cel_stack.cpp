#include "ui/cel_stack.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cassert>
#include <memory>

#include "ui/theme.h"
#include "ui/timeline_eye.h"

namespace snapper {
namespace {

constexpr int kCelHeight = 40;
constexpr int kIconSide = 16;
// A hidden cel slides this far aside.
constexpr int kHiddenSlide = 18;
// How far a lifted cel stands off the stack.
constexpr QPoint kLift{10, -5};
// Clear acetate: a faint cool sheen and a brighter edge.
constexpr QColor kSheet{220, 235, 245, 26};
constexpr QColor kSheetEdge{220, 235, 245, 80};
constexpr QColor kCelShadow{0, 0, 0, 110};

// The chip on a cel, one colour per kind of layer, in LayerContent's
// order: character, picture, text, effect, storyboard.
QColor KindColor(int kind) {
  assert(kind >= -1);
  constexpr QColor kKinds[] = {{0xf0, 0xc8, 0xaa}, {0x6a, 0x8f, 0xd6},
                               {0xec, 0xe6, 0xda}, {0xa0, 0x6a, 0xd0},
                               {0x8c, 0x86, 0x7e}};
  const bool is_known = kind >= 0 && kind < static_cast<int>(std::size(kKinds));
  return is_known ? kKinds[kind] : theme::kTextOff;
}

void PaintLock(const QRect& box, bool is_locked, QPainter* painter) {
  assert(painter != nullptr);
  assert(!box.isEmpty());
  const QColor ink = is_locked ? theme::kHandle : theme::kTextOff;
  painter->setPen(QPen(ink, 1.4));
  painter->setBrush(Qt::NoBrush);
  const QRectF body(box.left() + 3.0, box.top() + 7.0, box.width() - 6.0,
                    box.height() - 8.0);
  // An open lock's shackle swings up and to the side.
  const double lift = is_locked ? 0.0 : 3.0;
  const QRectF shackle(body.left() + 2.0 + lift, box.top() + 1.0 - lift,
                       body.width() - 4.0, 10.0);
  painter->drawArc(shackle, 0, 180 * 16);
  painter->drawLine(QPointF(shackle.right(), shackle.center().y()),
                    QPointF(shackle.right(), body.top()));
  if (is_locked) {
    painter->drawLine(QPointF(shackle.left(), shackle.center().y()),
                      QPointF(shackle.left(), body.top()));
  }
  painter->setBrush(is_locked ? ink : Qt::NoBrush);
  painter->drawRect(body);
}

// One cel in sheet, without its shadow.
void PaintCel(QPainter* painter, const QRect& sheet, const QModelIndex& index,
              bool is_picked) {
  assert(painter != nullptr);
  assert(index.isValid());
  const bool is_shown = index.data(kCelShownRole).toBool();
  painter->fillRect(sheet, kSheet);
  painter->setPen(QPen(is_picked ? theme::kPick : kSheetEdge,
                       is_picked ? 2.0 : 1.0));
  painter->setBrush(Qt::NoBrush);
  painter->drawRect(sheet.adjusted(0, 0, -1, -1));
  // The peg hole the cel hangs on.
  const QPoint peg(sheet.left() + 12, sheet.center().y());
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kFace);
  painter->drawEllipse(peg, 6, 6);
  const QRect chip(sheet.left() + 26, sheet.top() + 6, 24,
                   sheet.height() - 12);
  painter->fillRect(chip, KindColor(index.data(kCelKindRole).toInt()));
  const QRect eye = CelEyeRect(sheet);
  const QRect name(chip.right() + 8, sheet.top(), eye.left() - chip.right() - 14,
                   sheet.height());
  painter->setPen(is_shown ? theme::kText : theme::kTextOff);
  painter->drawText(name, Qt::AlignVCenter | Qt::AlignLeft,
                    painter->fontMetrics().elidedText(
                        index.data(Qt::DisplayRole).toString(),
                        Qt::ElideRight, name.width()));
  PaintEye(QRectF(eye), is_shown, painter);
  PaintLock(CelLockRect(sheet), index.data(kCelLockedRole).toBool(), painter);
}

QRect SheetOf(const QRect& row) {
  return row.adjusted(6, 3, -6, -4);
}

}  // namespace

QRect CelEyeRect(const QRect& row) {
  assert(row.width() > 2 * kIconSide);
  return QRect(row.right() - 2 * kIconSide - 14,
               row.center().y() - kIconSide / 2, kIconSide, kIconSide);
}

QRect CelLockRect(const QRect& row) {
  assert(row.width() > 2 * kIconSide);
  return QRect(row.right() - kIconSide - 8, row.center().y() - kIconSide / 2,
               kIconSide, kIconSide);
}

int CelDropIndex(int from_row, int slot, int count) {
  assert(from_row >= 0 && from_row < count);
  assert(slot >= 0 && slot <= count);
  const int to_row = slot > from_row ? slot - 1 : slot;
  return to_row == from_row ? -1 : count - 1 - to_row;
}

void CelDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                        const QModelIndex& index) const {
  assert(painter != nullptr);
  assert(index.isValid());
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  const bool is_shown = index.data(kCelShownRole).toBool();
  QRect sheet = SheetOf(option.rect);
  if (!is_shown) {
    sheet.translate(kHiddenSlide, 0);
    sheet.setRight(option.rect.right() - 6);
    painter->setOpacity(0.5);
  }
  painter->fillRect(sheet.translated(0, 3), kCelShadow);
  PaintCel(painter, sheet, index,
           option.state.testFlag(QStyle::State_Selected));
  painter->restore();
}

QSize CelDelegate::sizeHint(const QStyleOptionViewItem& option,
                            const QModelIndex& index) const {
  assert(index.isValid());
  return QSize(QStyledItemDelegate::sizeHint(option, index).width(),
               kCelHeight);
}

CelStack* CelStack::Install(QListWidget* list) {
  assert(list != nullptr);
  assert(list->viewport() != nullptr);
  // The list owns both.
  list->setItemDelegate(std::make_unique<CelDelegate>(list).release());
  return std::make_unique<CelStack>(list).release();
}

CelStack::CelStack(QListWidget* list)
    : QObject(list), list_(list), overlay_(new QWidget(list->viewport())) {
  assert(list_ != nullptr);
  list_->setSpacing(1);
  list_->setMouseTracking(true);
  list_->installEventFilter(this);
  list_->viewport()->installEventFilter(this);
  overlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
  overlay_->installEventFilter(this);
  overlay_->hide();
}

bool CelStack::eventFilter(QObject* watched, QEvent* event) {
  assert(event != nullptr);
  assert(list_ != nullptr);
  const QEvent::Type type = event->type();
  if (watched == overlay_) {
    if (type == QEvent::Paint) {
      PaintOverlay();
      return true;
    }
    return false;
  }
  const bool is_viewport = watched == list_->viewport();
  const auto* mouse = static_cast<const QMouseEvent*>(event);
  const bool is_left = is_viewport &&
                       (type == QEvent::MouseButtonPress ||
                        type == QEvent::MouseButtonRelease ||
                        type == QEvent::MouseMove);
  if (is_left && type == QEvent::MouseButtonPress &&
      mouse->button() == Qt::LeftButton) {
    return Press(mouse->position().toPoint());
  }
  if (is_left && type == QEvent::MouseMove &&
      mouse->buttons().testFlag(Qt::LeftButton)) {
    return Drag(mouse->position().toPoint());
  }
  if (is_left && type == QEvent::MouseButtonRelease) {
    return Drop(mouse->position().toPoint());
  }
  const bool is_escape =
      watched == list_ && type == QEvent::KeyPress && is_dragging_ &&
      static_cast<const QKeyEvent*>(event)->key() == Qt::Key_Escape;
  if (is_escape) {
    Cancel();
    return true;
  }
  return false;
}

bool CelStack::Press(const QPoint& at) {
  assert(list_ != nullptr);
  from_row_ = -1;
  const QModelIndex index = list_->indexAt(at);
  if (!index.isValid()) {
    return false;
  }
  const QRect row = SheetOf(list_->visualRect(index));
  const LayerId layer(index.data(kCelIdRole).toInt());
  if (CelEyeRect(row).adjusted(-4, -4, 4, 4).contains(at)) {
    emit ShownClicked(layer);
    return true;
  }
  if (CelLockRect(row).adjusted(-4, -4, 4, 4).contains(at)) {
    emit LockClicked(layer);
    return true;
  }
  from_row_ = index.row();
  press_at_ = at;
  // The pick still sees the press.
  return false;
}

bool CelStack::Drag(const QPoint& at) {
  assert(list_ != nullptr);
  const bool has_cel = from_row_ >= 0 && list_->count() > 1;
  if (!has_cel) {
    return false;
  }
  const bool is_far = (at - press_at_).manhattanLength() >=
                      QApplication::startDragDistance();
  if (!is_dragging_ && !is_far) {
    return false;
  }
  is_dragging_ = true;
  pointer_ = at;
  overlay_->setGeometry(list_->viewport()->rect());
  overlay_->show();
  overlay_->raise();
  overlay_->update();
  return true;
}

bool CelStack::Drop(const QPoint& at) {
  assert(list_ != nullptr);
  if (!is_dragging_) {
    from_row_ = -1;
    return false;
  }
  const int count = list_->count();
  const int index = CelDropIndex(from_row_, SlotAt(at.y()), count);
  const LayerId layer(list_->item(from_row_)->data(kCelIdRole).toInt());
  Cancel();
  if (index >= 0) {
    emit Moved(layer, index);
  }
  return true;
}

void CelStack::Cancel() {
  assert(overlay_ != nullptr);
  is_dragging_ = false;
  from_row_ = -1;
  overlay_->hide();
}

int CelStack::SlotAt(int y) const {
  assert(list_ != nullptr);
  const int count = list_->count();
  for (int row = 0; row < count; ++row) {
    const QRect box = list_->visualItemRect(list_->item(row));
    if (y < box.center().y()) {
      return row;
    }
  }
  return count;
}

void CelStack::PaintOverlay() {
  assert(overlay_ != nullptr);
  const bool has_cel = is_dragging_ && from_row_ >= 0 &&
                       from_row_ < list_->count();
  if (!has_cel) {
    return;
  }
  QPainter painter(overlay_);
  painter.setRenderHint(QPainter::Antialiasing);
  // The yellow slot where the cel would land.
  const int slot = SlotAt(pointer_.y());
  const int count = list_->count();
  const QRect edge =
      slot < count ? list_->visualItemRect(list_->item(slot))
                   : list_->visualItemRect(list_->item(count - 1));
  const int y = slot < count ? edge.top() : edge.bottom();
  painter.fillRect(QRect(6, y - 2, overlay_->width() - 12, 4), theme::kHandle);
  // The lifted cel follows the pointer, tilted a little, casting a long
  // shadow on the stack.
  const QModelIndex index = list_->model()->index(from_row_, 0);
  const QRect home = SheetOf(list_->visualRect(index));
  QRect sheet = home.translated(0, pointer_.y() - press_at_.y());
  sheet.translate(kLift);
  painter.fillRect(sheet.translated(4, 10), QColor(0, 0, 0, 140));
  painter.translate(sheet.center());
  painter.rotate(-1.2);
  painter.translate(-sheet.center());
  painter.fillRect(sheet, theme::kFace);
  PaintCel(&painter, sheet, index, true);
}

}  // namespace snapper
