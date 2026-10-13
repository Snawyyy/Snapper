#include "ui/flow_layout.h"

#include <QWidget>

#include <algorithm>
#include <cassert>

#include "ui/panel_kit.h"

namespace snapper {

FlowLayout::FlowLayout(QWidget* parent, int gap)
    : QLayout(parent), gap_(gap < 0 ? kTileGap : gap) {
  assert(gap_ >= 0);
  assert(gap >= -1);
  setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout() = default;

void FlowLayout::addItem(QLayoutItem* item) {
  assert(item != nullptr);
  assert(items_.size() < 10000);
  items_.emplace_back(item);
}

int FlowLayout::count() const {
  return static_cast<int>(items_.size());
}

QLayoutItem* FlowLayout::itemAt(int index) const {
  const bool is_inside = index >= 0 && index < count();
  return is_inside ? items_[static_cast<size_t>(index)].get() : nullptr;
}

QLayoutItem* FlowLayout::takeAt(int index) {
  const bool is_inside = index >= 0 && index < count();
  if (!is_inside) {
    return nullptr;
  }
  QLayoutItem* item = items_[static_cast<size_t>(index)].release();
  items_.erase(items_.begin() + index);
  return item;
}

Qt::Orientations FlowLayout::expandingDirections() const {
  return {};
}

bool FlowLayout::hasHeightForWidth() const {
  return true;
}

int FlowLayout::heightForWidth(int width) const {
  assert(width >= 0);
  return Place(QRect(0, 0, width, 0), false);
}

QSize FlowLayout::minimumSize() const {
  // As narrow as its widest item: everything else wraps.
  QSize least;
  for (const auto& item : items_) {
    least = least.expandedTo(item->minimumSize());
  }
  const QMargins margins = contentsMargins();
  return least + QSize(margins.left() + margins.right(),
                       margins.top() + margins.bottom());
}

QSize FlowLayout::sizeHint() const {
  // One line of everything, when there is room for it.
  int width = 0;
  int height = 0;
  for (const auto& item : items_) {
    const QSize hint = item->sizeHint();
    width += hint.width() + (width > 0 ? gap_ : 0);
    height = std::max(height, hint.height());
  }
  const QMargins margins = contentsMargins();
  return {width + margins.left() + margins.right(),
          height + margins.top() + margins.bottom()};
}

void FlowLayout::setGeometry(const QRect& rect) {
  QLayout::setGeometry(rect);
  Place(rect, true);
}

int FlowLayout::Place(const QRect& rect, bool is_placing) const {
  assert(gap_ >= 0);
  assert(rect.width() >= 0);
  const QRect area = rect.marginsRemoved(contentsMargins());
  int x = area.x();
  int y = area.y();
  int line_height = 0;
  for (const auto& item : items_) {
    const bool is_hidden =
        item->widget() != nullptr && item->widget()->isHidden();
    if (is_hidden) {
      continue;
    }
    const QSize hint = item->sizeHint();
    const bool is_wrapping =
        x > area.x() && x + hint.width() > area.right() + 1;
    if (is_wrapping) {
      x = area.x();
      y += line_height + gap_;
      line_height = 0;
    }
    if (is_placing) {
      item->setGeometry(QRect(QPoint(x, y), hint));
    }
    x += hint.width() + gap_;
    line_height = std::max(line_height, hint.height());
  }
  const QMargins margins = contentsMargins();
  return y + line_height - rect.y() + margins.bottom();
}

}  // namespace snapper
