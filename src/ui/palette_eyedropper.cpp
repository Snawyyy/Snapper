// PalettePanel: the eyedropper. While it is on the panel holds the
// mouse and keys: moving previews the color under the cursor in the
// current swatch, a left click picks it, Escape or a right click stops.

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPixmap>

#include <cassert>

#include "edit/palette_manager.h"
#include "ui/palette_panel.h"

namespace snapper {

void PalettePanel::StartEyedropper() {
  assert(palette_ != nullptr);
  assert(parentWidget() != nullptr);
  is_eyedropping_ = true;
  pick_.setChecked(true);
  grabMouse(Qt::CrossCursor);
  grabKeyboard();
}

void PalettePanel::StopEyedropper() {
  assert(palette_ != nullptr);
  assert(parentWidget() != nullptr);
  const bool was_on = is_eyedropping_;
  is_eyedropping_ = false;
  pick_.setChecked(false);
  if (was_on) {
    releaseMouse();
    releaseKeyboard();
  }
  ShowCurrent(palette_->current());
}

QColor PalettePanel::ColorAt(QPoint global) const {
  QWidget* window = parentWidget();
  assert(window != nullptr);
  assert(palette_ != nullptr);
  const QPoint at = window->mapFromGlobal(global);
  const bool is_inside = window->rect().contains(at);
  if (!is_inside) {
    return QColor();
  }
  // What the window draws there, stage and pictures included.
  const QImage pixel = window->grab(QRect(at, QSize(1, 1))).toImage();
  return pixel.isNull() ? QColor() : pixel.pixelColor(0, 0);
}

bool PalettePanel::Eyedrop(const QMouseEvent& event) {
  assert(palette_ != nullptr);
  assert(width() >= 0);
  if (!is_eyedropping_) {
    return false;
  }
  const QPoint global = event.globalPosition().toPoint();
  const QEvent::Type type = event.type();
  const bool is_move = type == QEvent::MouseMove;
  const bool is_press = type == QEvent::MouseButtonPress;
  if (is_move) {
    const QColor color = ColorAt(global);
    const bool is_color = color.isValid();
    if (is_color) {
      ShowCurrent(color);
    }
    return true;
  }
  const bool is_pick = is_press && event.button() == Qt::LeftButton;
  const bool is_cancel = is_press && !is_pick;
  if (is_pick) {
    const QColor color = ColorAt(global);
    StopEyedropper();
    const bool is_color = color.isValid();
    if (is_color) {
      palette_->Pick(color, event.modifiers());
    }
  } else if (is_cancel) {
    StopEyedropper();
  }
  return true;
}

void PalettePanel::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(palette_ != nullptr);
  const bool is_cancel = is_eyedropping_ && event->key() == Qt::Key_Escape;
  if (is_cancel) {
    StopEyedropper();
    return;
  }
  QWidget::keyPressEvent(event);
}

void PalettePanel::ShowCurrent(const QColor& color) {
  assert(color.isValid());
  assert(palette_ != nullptr);
  QPalette swatch = current_swatch_.palette();
  swatch.setColor(QPalette::Window, color);
  current_swatch_.setPalette(swatch);
  current_hex_.setText(color.name().toUpper());
}

}  // namespace snapper
