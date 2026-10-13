#include "ui/grip_splitter.h"

#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>

#include <cassert>
#include <memory>

#include "ui/snapper_style.h"

namespace snapper {

GripHandle::GripHandle(Qt::Orientation orientation, QSplitter* parent)
    : QSplitterHandle(orientation, parent) {
  assert(parent != nullptr);
  assert(!is_over_ && !is_down_);
}

void GripHandle::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  QPainter painter(this);
  painter.fillRect(rect(), palette().window());
  DrawGripLine(&painter, rect(), is_over_ || is_down_);
}

void GripHandle::enterEvent(QEnterEvent* event) {
  assert(event != nullptr);
  QSplitterHandle::enterEvent(event);
  is_over_ = true;
  update();
}

void GripHandle::leaveEvent(QEvent* event) {
  assert(event != nullptr);
  QSplitterHandle::leaveEvent(event);
  is_over_ = false;
  update();
}

void GripHandle::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  QSplitterHandle::mousePressEvent(event);
  is_down_ = true;
  update();
}

void GripHandle::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  QSplitterHandle::mouseReleaseEvent(event);
  is_down_ = false;
  update();
}

GripSplitter::GripSplitter(Qt::Orientation orientation, QWidget* parent)
    : QSplitter(orientation, parent) {
  assert(orientation == Qt::Horizontal || orientation == Qt::Vertical);
  assert(handleWidth() > 0);
}

QSplitterHandle* GripSplitter::createHandle() {
  assert(count() >= 0);
  // The splitter owns its handles.
  return std::make_unique<GripHandle>(orientation(), this).release();
}

}  // namespace snapper
