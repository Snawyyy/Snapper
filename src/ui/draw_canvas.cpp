#include "ui/draw_canvas.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "edit/draw_manager.h"

namespace snapper {
namespace {

constexpr double kFill = 0.8;
constexpr double kMinZoom = 0.05;
constexpr double kMaxZoom = 64.0;
// Each wheel notch zooms by this much.
constexpr double kNotchZoom = 1.25;
constexpr int kWheelUnit = 120;

bool IsHistoryKey(const QKeyEvent* event) {
  assert(event != nullptr);
  return event->matches(QKeySequence::Undo) ||
         event->matches(QKeySequence::Redo) ||
         (event->modifiers() == Qt::ControlModifier &&
          event->key() == Qt::Key_Y);
}

}  // namespace

DrawCanvas::DrawCanvas(const Managers& managers, QWidget* over)
    : QWidget(over), managers_(managers), over_(over), layout_(this),
      bar_(managers), tools_(managers) {
  assert(managers_.draw != nullptr);
  assert(over_ != nullptr);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setCursor(Qt::CrossCursor);
  setAttribute(Qt::WA_OpaquePaintEvent);
  layout_.setContentsMargins(0, 0, 0, 0);
  layout_.addWidget(&bar_);
  layout_.addLayout(&row_, 1);
  row_.addWidget(&tools_, 0, Qt::AlignTop | Qt::AlignLeft);
  row_.addStretch(1);
  // A click on the bars is theirs, never a stroke on the canvas.
  bar_.setAttribute(Qt::WA_NoMousePropagation);
  tools_.setAttribute(Qt::WA_NoMousePropagation);
  over_->installEventFilter(this);
  connect(managers_.draw, &DrawManager::Started, this, &DrawCanvas::Begin);
  connect(managers_.draw, &DrawManager::Ended, this, &DrawCanvas::End);
  connect(managers_.draw, &DrawManager::Changed, this, [this] { update(); });
  hide();
}

void DrawCanvas::Begin() {
  assert(over_ != nullptr);
  assert(managers_.draw->is_drawing());
  setGeometry(over_->rect());
  is_outside_ = false;
  is_stroking_ = false;
  show();
  raise();
  Fit();
  setFocus();
}

void DrawCanvas::End() {
  assert(over_ != nullptr);
  assert(!managers_.draw->is_drawing());
  is_stroking_ = false;
  is_panning_ = false;
  hide();
  over_->setFocus();
}

QTransform DrawCanvas::World() const {
  assert(zoom_ > 0.0);
  assert(std::isfinite(center_.x()) && std::isfinite(center_.y()));
  return QTransform::fromTranslate(-center_.x(), -center_.y()) *
         QTransform::fromScale(zoom_, zoom_) *
         QTransform::fromTranslate(width() / 2.0, height() / 2.0);
}

QPoint DrawCanvas::PixelAt(QPointF point) const {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(World().isInvertible());
  const QPointF at = World().inverted().map(point);
  return QPoint(static_cast<int>(std::floor(at.x())),
                static_cast<int>(std::floor(at.y())));
}

void DrawCanvas::Fit() {
  const DrawManager* draw = managers_.draw;
  assert(draw != nullptr);
  const QRectF box(draw->frame_rect());
  const bool is_sized = box.width() > 0.0 && box.height() > 0.0 &&
                        width() > 0 && height() > 0;
  zoom_ = is_sized ? std::clamp(kFill * std::min(width() / box.width(),
                                                  height() / box.height()),
                                kMinZoom, kMaxZoom)
                   : 1.0;
  center_ = box.center();
  update();
}

void DrawCanvas::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  DrawManager* draw = managers_.draw;
  event->accept();
  setFocus();
  // The middle button, or the left with Space held, grabs the view.
  const bool is_grab =
      event->button() == Qt::MiddleButton ||
      (is_space_ && event->button() == Qt::LeftButton);
  if (is_grab) {
    is_panning_ = true;
    pan_from_ = event->position();
    setCursor(Qt::ClosedHandCursor);
    return;
  }
  const bool is_draw_button = event->button() == Qt::LeftButton ||
                              event->button() == Qt::RightButton;
  if (!is_draw_button || !draw->is_drawing() || is_stroking_) {
    return;
  }
  const QPoint at = PixelAt(event->position());
  // Alt held picks with any tool; is_alt_ covers systems that drop the
  // modifier from the click.
  const bool is_alt =
      is_alt_ || event->modifiers().testFlag(Qt::AltModifier);
  if (is_alt) {
    draw->PickColor(at);
    return;
  }
  is_outside_ = !draw->image().rect().contains(at);
  is_stroking_ = true;
  draw->Press(at, event->button() == Qt::RightButton,
              event->modifiers().testFlag(Qt::ShiftModifier));
}

void DrawCanvas::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  event->accept();
  cursor_ = event->position();
  has_cursor_ = true;
  if (is_panning_) {
    center_ -= (event->position() - pan_from_) / zoom_;
    pan_from_ = event->position();
  } else if (is_stroking_) {
    const QPoint at = PixelAt(event->position());
    is_outside_ =
        is_outside_ || !managers_.draw->image().rect().contains(at);
    managers_.draw->MoveTo(at);
  }
  update();
}

void DrawCanvas::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  event->accept();
  if (is_panning_) {
    is_panning_ = false;
    setCursor(is_space_ ? Qt::OpenHandCursor : Qt::CrossCursor);
    return;
  }
  if (is_stroking_) {
    is_stroking_ = false;
    is_outside_ = false;
    managers_.draw->Release();
    update();
  }
}

void DrawCanvas::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  event->accept();
  const double notches =
      event->angleDelta().y() / static_cast<double>(kWheelUnit);
  if (notches == 0.0) {
    return;
  }
  const QPointF cursor = event->position();
  const QPointF under = World().inverted().map(cursor);
  zoom_ = std::clamp(zoom_ * std::pow(kNotchZoom, notches), kMinZoom,
                     kMaxZoom);
  center_ = under - (cursor - QPointF(width() / 2.0, height() / 2.0)) / zoom_;
  update();
}

void DrawCanvas::leaveEvent(QEvent* event) {
  assert(event != nullptr);
  has_cursor_ = false;
  update();
}

bool DrawCanvas::ToolKey(QKeyEvent* event) {
  assert(event != nullptr);
  DrawManager* draw = managers_.draw;
  const int key = event->key();
  switch (key) {
    case Qt::Key_B: draw->SetTool(DrawTool::kBrush); return true;
    case Qt::Key_E:
      // E flips the eraser on and off, back to the brush.
      draw->SetTool(draw->tool() == DrawTool::kEraser ? DrawTool::kBrush
                                                      : DrawTool::kEraser);
      return true;
    case Qt::Key_D: draw->SetOnion(!draw->is_onion()); return true;
    case Qt::Key_F: draw->SetTool(DrawTool::kFill); return true;
    case Qt::Key_I: draw->SetTool(DrawTool::kPicker); return true;
    case Qt::Key_V: draw->SetTool(DrawTool::kMove); return true;
    case Qt::Key_BracketLeft:
      draw->SetBrushSize(draw->brush_size() - 1);
      return true;
    case Qt::Key_BracketRight:
      draw->SetBrushSize(draw->brush_size() + 1);
      return true;
    case Qt::Key_R:
      if (!event->isAutoRepeat()) {
        draw->SetPeeking(true);
      }
      return true;
    case Qt::Key_1:
    case Qt::Key_Exclam: Fit(); return true;
    case Qt::Key_Return:
    case Qt::Key_Enter: bar_.Finish(); return true;
    case Qt::Key_Escape: bar_.AskCancel(); return true;
    default: return false;
  }
}

void DrawCanvas::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  DrawManager* draw = managers_.draw;
  // Every key stops here, so none reaches the stage underneath.
  event->accept();
  if (!draw->is_drawing()) {
    return;
  }
  if (event->key() == Qt::Key_Alt) {
    is_alt_ = true;
    update();
    return;
  }
  // Held Space turns the pointer into a hand that drags the view.
  const bool is_space_down =
      event->key() == Qt::Key_Space && !event->isAutoRepeat();
  if (is_space_down) {
    is_space_ = true;
    setCursor(Qt::OpenHandCursor);
    update();
    return;
  }
  if (event->matches(QKeySequence::Undo)) {
    draw->Undo();
    return;
  }
  if (IsHistoryKey(event)) {
    draw->Redo();
    return;
  }
  ToolKey(event);
}

void DrawCanvas::keyReleaseEvent(QKeyEvent* event) {
  assert(event != nullptr);
  event->accept();
  if (event->isAutoRepeat() || !managers_.draw->is_drawing()) {
    return;
  }
  if (event->key() == Qt::Key_R && managers_.draw->is_peeking()) {
    managers_.draw->SetPeeking(false);
  }
  if (event->key() == Qt::Key_Alt) {
    is_alt_ = false;
    update();
  }
  if (event->key() == Qt::Key_Space) {
    is_space_ = false;
    setCursor(is_panning_ ? Qt::ClosedHandCursor : Qt::CrossCursor);
    update();
  }
}

bool DrawCanvas::event(QEvent* event) {
  assert(event != nullptr);
  assert(managers_.draw != nullptr);
  const bool is_override = event->type() == QEvent::ShortcutOverride;
  if (is_override) {
    const auto* key = static_cast<QKeyEvent*>(event);
    const Qt::KeyboardModifiers held = key->modifiers();
    const bool is_plain =
        !held.testFlag(Qt::ControlModifier) && !held.testFlag(Qt::MetaModifier) &&
        (!held.testFlag(Qt::AltModifier) || key->key() == Qt::Key_Alt);
    // Tab is the window's full view key, drawing or not.
    const bool is_full_view = key->key() == Qt::Key_Tab;
    if ((is_plain && !is_full_view) || IsHistoryKey(key)) {
      event->accept();
      return true;
    }
  }
  return QWidget::event(event);
}

bool DrawCanvas::eventFilter(QObject* watched, QEvent* event) {
  assert(watched != nullptr);
  assert(event != nullptr);
  const bool is_resize = watched == over_ && event->type() == QEvent::Resize;
  if (is_resize && isVisible()) {
    setGeometry(over_->rect());
  }
  return QWidget::eventFilter(watched, event);
}

}  // namespace snapper
